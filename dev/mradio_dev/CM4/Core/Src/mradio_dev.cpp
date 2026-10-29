#include <cstdio>

#include "main.h"
#include "mmhal_os.h"
#include "mmlog.h"
#include "mmwlan.h"

extern "C" {
#include "mmregdb.h"
}

namespace mrover {
namespace {
constexpr char COUNTRY_CODE[] = "US";

void print_scan_result(const mmwlan_scan_result* result, void* /*arg*/) {
    std::printf("  %.*s rssi %d dBm bw %u MHz\n", static_cast<int>(result->ssid_len),
                reinterpret_cast<const char*>(result->ssid), static_cast<int>(result->rssi),
                static_cast<unsigned>(result->op_bw_mhz));
}

void print_scan_complete(mmwlan_scan_state state, void* /*arg*/) {
    std::printf("halow scan done, state %d\n", static_cast<int>(state));
}

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
    return true;
}

void scan_halow() {
    mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
    scan_req.scan_rx_cb = print_scan_result;
    scan_req.scan_complete_cb = print_scan_complete;
    const mmwlan_status status = mmwlan_scan_request(&scan_req);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: scan request failed %d\n", static_cast<int>(status));
    }
}
} // namespace

void init() {
    mm_logging_init();
    mmhal_init();
    if (boot_halow()) {
        scan_halow();
    }
}
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
