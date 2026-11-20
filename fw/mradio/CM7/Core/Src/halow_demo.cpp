#include <cstdint>
#include <cstdio>
#include <cstring>

#include "cmsis_os2.h"
#include "halow_radio.hpp"
#include "main.h"
#include "mmhal_os.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmping.h"

namespace mrover {
namespace {

// TODO: demo only, from the cmake, remove later
constexpr HalowRadio::Role ROLE = HALOW_DEFAULT_ROLE_AP != 0 ? HalowRadio::Role::ap : HalowRadio::Role::sta;

constexpr char AP_IP[] = "192.168.1.1";
constexpr char STA_IP[] = "192.168.1.2";

constexpr uint32_t PING_INTERVAL_MS = 1000;
constexpr uint32_t PING_POLL_MS = 50;
constexpr uint32_t BLINK_MS = 100;

constexpr bool is_ap() { return ROLE == HalowRadio::Role::ap; }
constexpr const char *role_name() { return is_ap() ? "ap" : "sta"; }
constexpr const char *local_ip() { return is_ap() ? AP_IP : STA_IP; }
constexpr const char *peer_ip() { return is_ap() ? STA_IP : AP_IP; }

constexpr HalowRadio::Config RADIO_CONFIG = {
    .role = ROLE,
    .ssid = "mradio_range",
    .passphrase = "12345678",
    .sub_band = halow::SubBand::mid,
    .bandwidth = halow::Bandwidth::mhz8,
    .local_ip = local_ip(),
    .gateway = AP_IP,
};
static_assert(halow::is_supported(RADIO_CONFIG.sub_band, RADIO_CONFIG.bandwidth));

// stack_size is in bytes
constexpr osThreadAttr_t LINK_MONITOR_ATTRIBUTES = {
    .name = "link_monitor",
    .stack_size = 1024 * 4,
    .priority = osPriorityNormal,
};

HalowRadio radio;

// D1, only one soldered (at least on this pcb)
void set_status_led(bool on) {
    if (on) {
        LL_GPIO_SetOutputPin(SoS_LED4_GPIO_Port, SoS_LED4_Pin);
    }
    else {
        LL_GPIO_ResetOutputPin(SoS_LED4_GPIO_Port, SoS_LED4_Pin);
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
    struct mmping_stats stats{};
    for (;;) {
        mmosal_task_sleep(PING_POLL_MS);
        mmping_stats(&stats);
        if (stats.ping_recv_count == replies_seen) {
            continue;
        }
        replies_seen = stats.ping_recv_count;
        const std::optional<int32_t> rssi = radio.rssi_dbm();
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
} // namespace

void init() {
    mm_logging_init();
    mmhal_init();
    std::printf("\nmradio halow demo, role %s, built " __DATE__ " " __TIME__ "\n", role_name());
    if (!radio.start(RADIO_CONFIG) || !start_ping()) {
        // solid D1 means fail
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
