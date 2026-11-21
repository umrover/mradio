#include <cstdint>
#include <cstdio>
#include <cstring>

#include "cmsis_os2.h"
#include "halow_radio.hpp"
#include "link_status.hpp"
#include "lwip/def.h"
#include "main.h"
#include "mmhal_os.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmping.h"
#include "web.hpp"
#include "wifi_radio.hpp"

namespace mrover {
namespace {

// TODO: demo only, from the cmake, remove later
constexpr HalowRadio::Role ROLE =
    HALOW_DEFAULT_ROLE_AP != 0 ? HalowRadio::Role::ap : HalowRadio::Role::sta;

// off the PC's 192.168.1.0/24 dev port, which the Ethernet side uses
constexpr char AP_IP[] = "192.168.2.1";
constexpr char STA_IP[] = "192.168.2.2";

constexpr uint32_t PING_INTERVAL_MS = 1000;
constexpr uint32_t PING_POLL_MS = 50;
constexpr uint32_t BLINK_MS = 100;
// association lingers for a while after a peer dies, so a link only counts as up while heartbeats
// come back, and three missed in a row take it down
constexpr uint32_t LINK_STALE_MS = 3 * PING_INTERVAL_MS;

bool heard_recently(std::optional<uint32_t> last_reply_ms) {
    return last_reply_ms.has_value() && HAL_GetTick() - *last_reply_ms <= LINK_STALE_MS;
}

constexpr bool is_ap() { return ROLE == HalowRadio::Role::ap; }
constexpr const char *role_name() { return is_ap() ? "ap" : "sta"; }
constexpr const char *local_ip() { return is_ap() ? AP_IP : STA_IP; }
constexpr const char *peer_ip() { return is_ap() ? STA_IP : AP_IP; }

constexpr char WIFI_AP_IP[] = "192.168.3.1";
constexpr char WIFI_STA_IP[] = "192.168.3.2";
constexpr uint32_t WIFI_PING_TIMEOUT_MS = 500;

constexpr config::Net ETH_NET = {
    .ip = LWIP_MAKEU32(192, 168, 1, is_ap() ? 10 : 11),
    .mask = LWIP_MAKEU32(255, 255, 255, 0),
};

constexpr HalowRadio::Config RADIO_CONFIG = {
    .role = ROLE,
    .ssid = "mradio_range",
    .passphrase = "12345678",
    .sub_band = halow::SubBand::mid,
    .bandwidth = halow::Bandwidth::mhz8,
    .local_ip = local_ip(),
    .gateway = AP_IP,
};

// off while the Sona card connectors are unreliable, a failed bring-up starves HaLow and Ethernet
constexpr bool WIFI_ENABLED = false;

// the channel comes from the saved wifi_5 config in start_wifi
constexpr WifiRadio::Config WIFI_CONFIG = {
    .role = is_ap() ? WifiRadio::Role::ap : WifiRadio::Role::sta,
    .ssid = "mradio_wifi",
    .passphrase = "12345678",
    .band = WifiRadio::Band::ghz5,
    .local_ip = is_ap() ? WIFI_AP_IP : WIFI_STA_IP,
    .gateway = WIFI_AP_IP,
};
constexpr config::Link WIFI_LINK =
    WIFI_CONFIG.band == WifiRadio::Band::ghz5 ? config::Link::wifi_5 : config::Link::wifi_2_4;

// stack_size is in bytes
constexpr osThreadAttr_t LINK_MONITOR_ATTRIBUTES = {
    .name = "link_monitor",
    .stack_size = 1024 * 4,
    .priority = osPriorityNormal,
};

constexpr osThreadAttr_t WIFI_MONITOR_ATTRIBUTES = {
    .name = "wifi_monitor",
    .stack_size = 1024 * 4,
    .priority = osPriorityNormal,
};

HalowRadio radio;
WifiRadio wifi;

// D1 is the only LED fitted on these boards, so HaLow status borrows the system LED
void set_status_led(bool on) {
    if (on) {
        LL_GPIO_SetOutputPin(LED_SYSTEM_GPIO_Port, LED_SYSTEM_Pin);
    }
    else {
        LL_GPIO_ResetOutputPin(LED_SYSTEM_GPIO_Port, LED_SYSTEM_Pin);
    }
}

bool start_ping() {
    mmping_args args = MMPING_ARGS_DEFAULT;
    std::strncpy(args.ping_src, local_ip(), sizeof args.ping_src - 1);
    std::strncpy(args.ping_target, peer_ip(), sizeof args.ping_target - 1);
    args.ping_interval_ms = PING_INTERVAL_MS;
    if (mmping_start(&args) == 0) {
        std::printf("halow: ping start failed\n");
        return false;
    }
    std::printf("halow: %s %s pinging %s every %lu ms\n", role_name(), local_ip(), peer_ip(),
                static_cast<unsigned long>(PING_INTERVAL_MS));
    return true;
}

[[noreturn]] void link_monitor_task(void * /*argument*/) {
    uint32_t replies_seen = 0;
    std::optional<uint32_t> last_reply_ms;
    struct mmping_stats stats{};
    for (;;) {
        mmosal_task_sleep(PING_POLL_MS);
        mmping_stats(&stats);
        if (stats.ping_recv_count != replies_seen) {
            last_reply_ms = HAL_GetTick();
        }
        link_status::set_up(config::Link::halow, radio.is_peer_up() && heard_recently(last_reply_ms));
        link_status::record_heartbeats(
            config::Link::halow, stats.ping_total_count, stats.ping_recv_count,
            stats.ping_recv_count > 0 ? std::optional{stats.ping_avg_time_ms} : std::nullopt);
        if (stats.ping_recv_count == replies_seen) {
            continue;
        }
        replies_seen = stats.ping_recv_count;
        const std::optional<int32_t> rssi = radio.rssi_dbm();
        link_status::set_rssi(config::Link::halow, rssi);
        std::printf("halow: reply %lu/%lu avg %lu ms", static_cast<unsigned long>(replies_seen),
                    static_cast<unsigned long>(stats.ping_total_count),
                    static_cast<unsigned long>(stats.ping_avg_time_ms));
        if (rssi.has_value()) {
            std::printf(" rssi %ld dBm", static_cast<long>(*rssi));
        }
        std::printf("\n");
        set_status_led(true);
        mmosal_task_sleep(BLINK_MS);
        set_status_led(false);
    }
}
[[noreturn]] void wifi_monitor_task(void * /*argument*/) {
    const char *const peer = is_ap() ? WIFI_STA_IP : WIFI_AP_IP;
    uint32_t sent = 0;
    uint32_t replies = 0;
    std::optional<uint32_t> last_reply_ms;
    for (;;) {
        osDelay(PING_INTERVAL_MS);
        link_status::set_up(WIFI_LINK, wifi.is_peer_up() && heard_recently(last_reply_ms));
        if (!wifi.is_peer_up()) {
            continue;
        }
        ++sent;
        const std::optional<uint32_t> rtt_ms = wifi.ping(peer, WIFI_PING_TIMEOUT_MS);
        if (rtt_ms.has_value()) {
            last_reply_ms = HAL_GetTick();
        }
        const std::optional<int16_t> rssi = wifi.rssi_dbm();
        link_status::record_heartbeats(WIFI_LINK, sent, replies + (rtt_ms ? 1 : 0), rtt_ms);
        link_status::set_rssi(WIFI_LINK, rssi);
        if (!rtt_ms.has_value()) {
            std::printf("wifi: no reply from %s, %lu/%lu\n", peer,
                        static_cast<unsigned long>(replies), static_cast<unsigned long>(sent));
            continue;
        }
        ++replies;
        std::printf("wifi: reply %lu/%lu %lu ms", static_cast<unsigned long>(replies),
                    static_cast<unsigned long>(sent), static_cast<unsigned long>(*rtt_ms));
        if (rssi.has_value()) {
            std::printf(" rssi %d dBm", static_cast<int>(*rssi));
        }
        std::printf("\n");
    }
}

void start_wifi() {
    WifiRadio::Config wifi_config = WIFI_CONFIG;
    // config validates the channel range, so it fits a byte
    wifi_config.channel = static_cast<uint8_t>(config::current().wifi_5.channel);
    if (!wifi.start(wifi_config)) {
        std::printf("wifi: not started\n");
        return;
    }
    if (osThreadNew(wifi_monitor_task, nullptr, &WIFI_MONITOR_ATTRIBUTES) == nullptr) {
        std::printf("wifi: monitor task create failed\n");
    }
}
} // namespace

void init() {
    mm_logging_init();
    mmhal_init();
    std::printf("\nmradio halow demo, role %s, built " __DATE__ " " __TIME__ "\n", role_name());
    if (!radio.start(RADIO_CONFIG)) {
        // solid D1 means fail
        set_status_led(true);
        return;
    }
    // radio.start brings up tcpip, which web needs
    if (!web::init(ETH_NET)) {
        std::printf("web: not started\n");
    }
    if (WIFI_ENABLED) {
        start_wifi();
    }
    if (!start_ping()) {
        set_status_led(true);
        return;
    }
    if (osThreadNew(link_monitor_task, nullptr, &LINK_MONITOR_ATTRIBUTES) == nullptr) {
        std::printf("halow: link monitor task create failed\n");
        set_status_led(true);
    }
}
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
