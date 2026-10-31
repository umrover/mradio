#include "web.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "FreeRTOS.h"
#include "config.hpp"
#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "portable.h"
#include "stm32h7xx_hal.h"

extern "C" {
extern struct netif gnetif;

// symbols that mark the start and end of binary html in flash
extern const char _binary_index_html_start[];
extern const char _binary_index_html_end[];
}

namespace mrover::web {
namespace {

constexpr std::size_t MAX_POST_BODY_BYTES = 1024;
constexpr std::string_view POST_RESPONSE_URI = "/api/.post";

using ArduinoJson::JsonDocument;
using ArduinoJson::JsonObject;

struct PostSlot {
    void *connection = nullptr;
    bool reboot = false;
    std::size_t content_length = 0;
    std::string body;
};

std::vector<PostSlot> post_slots;
// httpd opens the response file inside the same httpd_post_finished call, so one slot suffices
std::unique_ptr<char[]> pending_post_response;
std::vector<std::string_view> reset_reasons;

std::string_view index_html() {
    const auto start = reinterpret_cast<std::uintptr_t>(_binary_index_html_start);
    const auto end = reinterpret_cast<std::uintptr_t>(_binary_index_html_end);
    return {_binary_index_html_start, end - start};
}

std::string_view reason_phrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 409:
        return "Conflict";
    case 502:
        return "Bad Gateway";
    default:
        return "Internal Server Error";
    }
}

void write_error(JsonDocument &json, const config::ApiError &error) {
    json["error"] = error.code;
    if (error.field.empty()) {
        json["field"] = nullptr;
    } else {
        json["field"] = error.field;
    }
    json["reason"] = error.reason;
}

// Returns the header and body as one NUL terminated buffer for open_response to hand to httpd
template <typename BodyWriter>
std::unique_ptr<char[]> make_response(int status, BodyWriter &&write_body) {
    JsonDocument json;
    write_body(json);
    if (json.overflowed()) {
        status = 500;
        json.clear();
        write_error(json, config::make_error(500, "internal", "", "out of memory"));
    }
    const std::size_t body_size = measureJson(json);

    const auto write_header = [&](char *out, std::size_t size) {
        return std::snprintf(
            out, size,
            "HTTP/1.0 %d %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
            "Cache-Control: no-store\r\n\r\n",
            status, reason_phrase(status).data(), static_cast<unsigned>(body_size));
    };
    const auto header_size = static_cast<std::size_t>(write_header(nullptr, 0));
    auto bytes = std::make_unique_for_overwrite<char[]>(header_size + body_size + 1);
    write_header(bytes.get(), header_size + 1);
    serializeJson(json, bytes.get() + header_size, body_size + 1);
    return bytes;
}

std::unique_ptr<char[]> error_response(const config::ApiError &error) {
    return make_response(error.status, [&error](JsonDocument &json) { write_error(json, error); });
}

std::string_view eth_state() {
    if (!netif_is_link_up(&gnetif)) {
        return "down";
    }
    const bool full_duplex = (ETH->MACCR & ETH_MACCR_DM) != 0;
    const bool fast = (ETH->MACCR & ETH_MACCR_FES) != 0;
    if (fast) {
        return full_duplex ? "100F" : "100H";
    }
    return full_duplex ? "10F" : "10H";
}

void write_link_state(JsonObject link_json, config::Link link, std::uint64_t uptime_ms) {
    link_json["state"] = config::link_up(link) ? "up" : "down";
    link_json["state_s"] = uptime_ms / 1000;
}

void write_heartbeats(JsonObject link_json) {
    link_json["rtt_ms"] = nullptr;
    link_json["heartbeats_sent"] = 0;
    link_json["heartbeats_acked"] = 0;
}

void write_local_link(JsonObject link_json) {
    link_json["rssi_dbm"] = nullptr;
    link_json["mcs"] = nullptr;
    link_json["tx_bytes"] = 0;
    link_json["rx_bytes"] = 0;
    link_json["tx_packets"] = nullptr;
    link_json["tx_failed"] = nullptr;
    link_json["txq_depth"] = 0;
    link_json["txq_drops"] = 0;
}

void write_status(JsonDocument &json) {
    const config::Config &current = config::current();
    const std::uint64_t uptime_ms = HAL_GetTick();

    const JsonObject halow = json["links"]["halow"].to<JsonObject>();
    write_link_state(halow, config::Link::halow, uptime_ms);
    halow["sub_band"] = config::sub_band_name(current.halow.sub_band);
    halow["bw_mhz"] = current.halow.bw_mhz;
    write_heartbeats(halow);

    const auto wifi_links = {std::tuple{"wifi_2_4", config::Link::wifi_2_4, current.wifi_2_4},
                             std::tuple{"wifi_5", config::Link::wifi_5, current.wifi_5}};
    for (const auto &[name, link, settings] : wifi_links) {
        const JsonObject wifi = json["links"][name].to<JsonObject>();
        write_link_state(wifi, link, uptime_ms);
        wifi["channel"] = settings.channel;
        wifi["bw_mhz"] = settings.bw_mhz;
        write_heartbeats(wifi);
    }

    const JsonObject local = json["local"].to<JsonObject>();
    local["role"] = config::role_name(config::boot_role());
    local["ip"] = config::format_ipv4(lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(&gnetif)))).data();
    local["uptime_ms"] = uptime_ms;
    local["fw"] = MRADIO_FW_VERSION;
    write_local_link(local["links"]["halow"].to<JsonObject>());
    write_local_link(local["links"]["wifi_2_4"].to<JsonObject>());
    write_local_link(local["links"]["wifi_5"].to<JsonObject>());

    const JsonObject system = local["system"].to<JsonObject>();
    // TODO: wait a second do we need a separate temp sensor
    system["temp_c"] = nullptr;
    system["heap_free"] = xPortGetFreeHeapSize();
    system["heap_min"] = xPortGetMinimumEverFreeHeapSize();
    system["cm7_idle_us"] = nullptr;
    system["cm4_idle_us"] = nullptr;
    system["eth"] = eth_state();
    const ArduinoJson::JsonArray reasons = system["reset_reason"].to<ArduinoJson::JsonArray>();
    for (const std::string_view reason : reset_reasons) {
        reasons.add(reason);
    }

    json["peer"] = nullptr;
}

void capture_reset_reasons() {
    const auto note = [](bool flagged, std::string_view reason) {
        if (flagged) {
            reset_reasons.push_back(reason);
        }
    };
    note(__HAL_RCC_C1_GET_FLAG(RCC_FLAG_PORRST) != 0U, "power_on");
    note(__HAL_RCC_C1_GET_FLAG(RCC_FLAG_PINRST) != 0U, "pin");
    note(__HAL_RCC_C1_GET_FLAG(RCC_FLAG_SFTR1ST) != 0U ||
             __HAL_RCC_C1_GET_FLAG(RCC_FLAG_SFTR2ST) != 0U,
         "software");
    note(__HAL_RCC_C1_GET_FLAG(RCC_FLAG_IWDG1RST) != 0U ||
             __HAL_RCC_C1_GET_FLAG(RCC_FLAG_IWDG2RST) != 0U ||
             __HAL_RCC_C1_GET_FLAG(RCC_FLAG_WWDG1RST) != 0U ||
             __HAL_RCC_C1_GET_FLAG(RCC_FLAG_WWDG2RST) != 0U,
         "watchdog");
    note(__HAL_RCC_C1_GET_FLAG(RCC_FLAG_BORRST) != 0U, "brownout");
    __HAL_RCC_C1_CLEAR_RESET_FLAGS();
}

void reboot_now(void * /*arg*/) { NVIC_SystemReset(); }

std::unique_ptr<char[]> handle_post(bool reboot, std::string_view body) {
    if (reboot) {
        // Long enough for the reboot response to leave the TCP send buffer first
        sys_timeout(500, reboot_now, nullptr);
        return make_response(200, [](JsonDocument &json) { json["ok"] = true; });
    }
    if (const auto error = config::apply_patch(body)) {
        return error_response(*error);
    }
    return make_response(200, [](JsonDocument &json) {
        config::write_json(json.to<ArduinoJson::JsonVariant>(), config::current());
    });
}

void set_response_uri(char *response_uri, u16_t response_uri_len) {
    std::snprintf(response_uri, response_uri_len, "%.*s",
                  static_cast<int>(POST_RESPONSE_URI.size()), POST_RESPONSE_URI.data());
}

err_t reject_post(const config::ApiError &error, char *response_uri, u16_t response_uri_len) {
    pending_post_response = error_response(error);
    set_response_uri(response_uri, response_uri_len);
    return ERR_ARG;
}

int open_response(fs_file *file, std::unique_ptr<char[]> response) {
    if (!response) {
        return 0;
    }
    file->len = static_cast<int>(std::strlen(response.get()));
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
    file->data = response.release();
    return 1;
}

} // namespace

void init() {
    capture_reset_reasons();
    LOCK_TCPIP_CORE();
    config::init(config::Net{.ip = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(&gnetif))),
                             .mask = lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(&gnetif)))});
    httpd_init();
    UNLOCK_TCPIP_CORE();
}

} // namespace mrover::web

// lwIP httpd hooks, all called on the tcpip thread

int fs_open_custom(struct fs_file *file, const char *name) {
    using namespace mrover::web;
    const std::string_view path{name};
    if (path == "/index.html") {
        const std::string_view page = index_html();
        file->data = page.data();
        file->len = static_cast<int>(page.size());
        file->index = file->len;
        file->flags = 0;
        return 1;
    }
    if (path == "/api/config") {
        return open_response(file, make_response(200, [](JsonDocument &json) {
                                 mrover::config::write_json(json.to<ArduinoJson::JsonVariant>(),
                                                            mrover::config::current());
                             }));
    }
    if (path == "/api/status") {
        return open_response(file, make_response(200, write_status));
    }
    if (path == POST_RESPONSE_URI) {
        return open_response(file, std::move(pending_post_response));
    }
    if (path == "/404.html") {
        return open_response(file, error_response(mrover::config::make_error(404, "not_found", "", "no such path")));
    }
    return 0;
}

void fs_close_custom(struct fs_file *file) {
    if (file->data != mrover::web::index_html().data()) {
        const std::unique_ptr<char[]> owned{const_cast<char *>(file->data)};
    }
}

err_t httpd_post_begin(void *connection, const char *uri, const char * /*http_request*/,
                       u16_t /*http_request_len*/, int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd) {
    using namespace mrover::web;
    const std::string_view path{uri};
    const bool reboot = path == "/api/reboot";
    if (!reboot && path != "/api/config") {
        return reject_post(mrover::config::make_error(404, "not_found", "", "no such path"), response_uri,
                           response_uri_len);
    }
    if (content_len < 0 || static_cast<std::size_t>(content_len) > MAX_POST_BODY_BYTES) {
        return reject_post(mrover::config::make_error(400, "bad_json", "", "body larger than 1024 bytes"), response_uri,
                           response_uri_len);
    }
    if (post_slots.size() >= 2) {
        return reject_post(mrover::config::make_error(500, "internal", "", "too many requests in flight"), response_uri,
                           response_uri_len);
    }
    post_slots.push_back(PostSlot{.connection = connection,
                                  .reboot = reboot,
                                  .content_length = static_cast<std::size_t>(content_len),
                                  .body = {}});
    *post_auto_wnd = 1;
    return ERR_OK;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    if (p == nullptr) {
        return ERR_OK;
    }
    using namespace mrover::web;
    const auto slot = std::ranges::find(post_slots, connection, &PostSlot::connection);
    err_t result = ERR_VAL;
    if (slot != post_slots.end()) {
        const std::size_t received = slot->body.size();
        const std::size_t count = std::min<std::size_t>(p->tot_len, slot->content_length - received);
        slot->body.resize(received + count);
        pbuf_copy_partial(p, slot->body.data() + received, static_cast<u16_t>(count), 0);
        result = ERR_OK;
    }
    pbuf_free(p);
    return result;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    using namespace mrover::web;
    const auto slot = std::ranges::find(post_slots, connection, &PostSlot::connection);
    if (slot == post_slots.end()) {
        return;
    }
    const PostSlot finished = std::move(*slot);
    post_slots.erase(slot);
    // httpd also calls this when the client disconnects mid body, and then nobody reads a response
    if (finished.body.size() == finished.content_length) {
        pending_post_response = handle_post(finished.reboot, finished.body);
        set_response_uri(response_uri, response_uri_len);
    }
}
