#include <cstdio>
#include <cstring>

#include "link_test.hpp"
#include "main.h"
#include "mmhal_os.h"
#include "mmipal.h"
#include "mmlog.h"
#include "mmwlan.h"

extern "C" {
#include "mmregdb.h"
}

namespace mrover {
namespace {
constexpr char COUNTRY_CODE[] = "US";

constexpr char HALOW_SSID[] = "mradio_range";
constexpr char HALOW_PASSPHRASE[] = "12345678";

constexpr char LOCAL_IP[] = "192.168.1.2";
constexpr char NETMASK[] = "255.255.255.0";
constexpr char AP_IP[] = "192.168.1.1";

const char *sta_state_name(mmwlan_sta_state state) {
    switch (state) {
    case MMWLAN_STA_DISABLED:
        return "disabled";
    case MMWLAN_STA_CONNECTING:
        return "connecting";
    case MMWLAN_STA_CONNECTED:
        return "connected";
    }
    return "unknown";
}

void print_sta_state(mmwlan_sta_state state) { std::printf("halow: sta %s\n", sta_state_name(state)); }

bool boot_halow() {
    mmwlan_init();

    const mmwlan_s1g_channel_list* const channel_list =
        mmwlan_lookup_regulatory_domain(get_regulatory_db(), COUNTRY_CODE);
    if (channel_list == nullptr) {
        std::printf("halow: no channel list for %s\n", COUNTRY_CODE);
        return false;
    }

    mmwlan_status status = mmwlan_set_channel_list(channel_list);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: set channel list failed %d\n", static_cast<int>(status));
        return false;
    }

    const mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    status = mmwlan_boot(&boot_args);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: boot failed %d\n", static_cast<int>(status));
        return false;
    }

    mmwlan_version version{};
    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: get version failed %d\n", static_cast<int>(status));
        return false;
    }
    std::printf("halow: morselib %s fw %s chip 0x%04lx %s\n", version.morselib_version,
                version.morse_fw_version, static_cast<unsigned long>(version.morse_chip_id),
                version.morse_chip_id_string);

    if (HALOW_TX_POWER_DBM != 0) {
        status = mmwlan_override_max_tx_power(HALOW_TX_POWER_DBM);
        std::printf("halow: max tx power %d dBm, status %d\n", HALOW_TX_POWER_DBM, static_cast<int>(status));
    }
    return true;
}

void print_link_status(const mmipal_link_status *link_status) {
    std::printf("link %s, ip %s\n", link_status->link_state == MMIPAL_LINK_UP ? "up" : "down",
                link_status->ip_addr);
    link_test::set_peer_up(link_status->link_state == MMIPAL_LINK_UP);
}

bool start_ip() {
    mmipal_init_args args = MMIPAL_INIT_ARGS_DEFAULT;
    args.mode = MMIPAL_STATIC;
    std::strncpy(args.ip_addr, LOCAL_IP, sizeof args.ip_addr - 1);
    std::strncpy(args.netmask, NETMASK, sizeof args.netmask - 1);
    std::strncpy(args.gateway_addr, AP_IP, sizeof args.gateway_addr - 1);
    if (mmipal_init(&args) != MMIPAL_SUCCESS) {
        std::printf("link test: ip init failed\n");
        return false;
    }
    mmipal_set_link_status_callback(print_link_status);
    return true;
}

bool join_halow() {
    mmwlan_sta_args args = MMWLAN_STA_ARGS_INIT;
    static_assert(sizeof HALOW_SSID - 1 <= sizeof args.ssid);
    static_assert(sizeof HALOW_PASSPHRASE <= sizeof args.passphrase);
    std::memcpy(args.ssid, HALOW_SSID, sizeof HALOW_SSID - 1);
    args.ssid_len = sizeof HALOW_SSID - 1;
    args.security_type = MMWLAN_SAE;
    std::memcpy(args.passphrase, HALOW_PASSPHRASE, sizeof HALOW_PASSPHRASE);
    args.passphrase_len = sizeof HALOW_PASSPHRASE - 1;

    mmwlan_status status = mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: power save disable failed %d\n", static_cast<int>(status));
        return false;
    }
    status = mmwlan_sta_enable(&args, print_sta_state);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: sta enable failed %d\n", static_cast<int>(status));
        return false;
    }
    return true;
}
} // namespace

void init() {
    mm_logging_init();
    mmhal_init();
    std::printf("\nmradio_dev link test, built " __DATE__ " " __TIME__ "\n");
    if (boot_halow() && start_ip() && link_test::start(LOCAL_IP, AP_IP, LINK_TEST_FLOOD != 0)) {
        static_cast<void>(join_halow());
    }
}
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
