#include <cstdint>
#include <cstdio>
#include <cstring>

#include "link_test.hpp"
#include "main.h"
#include "mmhal_os.h"
#include "mmipal.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmwlan.h"

extern "C" {
#include "mmregdb.h"
}

namespace mrover {
enum class HalowRole : uint8_t { ap, sta };

// TODO: demo only, from the cmake, remove later
HalowRole halow_role = HALOW_DEFAULT_ROLE_AP != 0 ? HalowRole::ap : HalowRole::sta;

namespace {

constexpr char COUNTRY_CODE[] = "US";
constexpr char HALOW_SSID[] = "mradio_range";
constexpr char HALOW_PASSPHRASE[] = "12345678";

constexpr uint16_t AP_OP_CLASS = 71;
constexpr uint16_t AP_S1G_CHANNEL = 44;

constexpr char AP_IP[] = "192.168.1.1";
constexpr char STA_IP[] = "192.168.1.2";
constexpr char NETMASK[] = "255.255.255.0";

bool is_ap() { return halow_role == HalowRole::ap; }
const char *role_name() { return is_ap() ? "ap" : "sta"; }
const char *local_ip() { return is_ap() ? AP_IP : STA_IP; }
const char *peer_ip() { return is_ap() ? STA_IP : AP_IP; }

// D1, only one soldered (at least on this pcb)
void set_status_led(bool on) {
    if (on) {
        LL_GPIO_SetOutputPin(SoS_LED4_GPIO_Port, SoS_LED4_Pin);
    }
    else {
        LL_GPIO_ResetOutputPin(SoS_LED4_GPIO_Port, SoS_LED4_Pin);
    }
}

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

const char *ap_sta_state_name(mmwlan_ap_sta_state state) {
    switch (state) {
    case MMWLAN_AP_STA_UNKNOWN:
        return "gone";
    case MMWLAN_AP_STA_AUTHENTICATED:
        return "authenticated";
    case MMWLAN_AP_STA_ASSOCIATED:
        return "associated";
    case MMWLAN_AP_STA_AUTHORIZED:
        return "authorized";
    }
    return "unknown";
}

void print_sta_state(mmwlan_sta_state state) {
    std::printf("halow: sta %s\n", sta_state_name(state));
}

void print_ap_sta_status(const mmwlan_ap_sta_status *sta_status, void * /*arg*/) {
    const uint8_t *const mac = sta_status->mac_addr;
    std::printf("halow: station %02x:%02x:%02x:%02x:%02x:%02x aid %u %s\n", mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5], static_cast<unsigned>(sta_status->aid),
                ap_sta_state_name(sta_status->state));
    link_test::set_peer_up(sta_status->state == MMWLAN_AP_STA_AUTHORIZED);
}

void print_link_status(const mmipal_link_status *link_status) {
    std::printf("halow: link %s, ip %s\n",
                link_status->link_state == MMIPAL_LINK_UP ? "up" : "down", link_status->ip_addr);
    // the ap side tracks its peer through the station status callback instead
    if (!is_ap()) {
        link_test::set_peer_up(link_status->link_state == MMIPAL_LINK_UP);
    }
}

bool boot_halow() {
    mmwlan_init();

    const mmwlan_s1g_channel_list *const channel_list =
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

    mmwlan_bcf_metadata bcf{};
    if (mmwlan_get_bcf_metadata(&bcf) == MMWLAN_SUCCESS) {
        std::printf("halow: bcf %s %s\n", bcf.board_desc, bcf.build_version);
    }
    return true;
}

bool start_ip() {
    mmipal_init_args args = MMIPAL_INIT_ARGS_DEFAULT;
    args.mode = MMIPAL_STATIC;
    args.ip6_mode = MMIPAL_IP6_DISABLED;
    std::strncpy(args.ip_addr, local_ip(), sizeof args.ip_addr - 1);
    std::strncpy(args.netmask, NETMASK, sizeof args.netmask - 1);
    std::strncpy(args.gateway_addr, AP_IP, sizeof args.gateway_addr - 1);
    if (mmipal_init(&args) != MMIPAL_SUCCESS) {
        std::printf("halow: ip init failed\n");
        return false;
    }
    mmipal_set_link_status_callback(print_link_status);
    return true;
}

bool start_ap() {
    mmwlan_ap_args args = MMWLAN_AP_ARGS_INIT;
    static_assert(sizeof HALOW_SSID - 1 <= sizeof args.ssid);
    static_assert(sizeof HALOW_PASSPHRASE <= sizeof args.passphrase);
    std::memcpy(args.ssid, HALOW_SSID, sizeof HALOW_SSID - 1);
    args.ssid_len = sizeof HALOW_SSID - 1;
    args.security_type = MMWLAN_SAE;
    std::memcpy(args.passphrase, HALOW_PASSPHRASE, sizeof HALOW_PASSPHRASE);
    args.passphrase_len = sizeof HALOW_PASSPHRASE - 1;
    args.op_class = AP_OP_CLASS;
    args.s1g_chan_num = AP_S1G_CHANNEL;
    args.sta_status_cb = print_ap_sta_status;

    const mmwlan_status status = mmwlan_ap_enable(&args);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: ap enable failed %d\n", static_cast<int>(status));
        return false;
    }
    std::printf("halow: ap %s up on op class %u channel %u\n", HALOW_SSID,
                static_cast<unsigned>(AP_OP_CLASS), static_cast<unsigned>(AP_S1G_CHANNEL));
    return true;
}

bool start_sta() {
    mmwlan_sta_args args = MMWLAN_STA_ARGS_INIT;
    static_assert(sizeof HALOW_SSID - 1 <= sizeof args.ssid);
    static_assert(sizeof HALOW_PASSPHRASE <= sizeof args.passphrase);
    std::memcpy(args.ssid, HALOW_SSID, sizeof HALOW_SSID - 1);
    args.ssid_len = sizeof HALOW_SSID - 1;
    args.security_type = MMWLAN_SAE;
    std::memcpy(args.passphrase, HALOW_PASSPHRASE, sizeof HALOW_PASSPHRASE);
    args.passphrase_len = sizeof HALOW_PASSPHRASE - 1;

    const mmwlan_status status = mmwlan_sta_enable(&args, print_sta_state);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: sta enable failed %d\n", static_cast<int>(status));
        return false;
    }
    return true;
}

bool start_halow() {
    if (!boot_halow() || !start_ip()) {
        return false;
    }
    // prevent sleep
    const mmwlan_status status = mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: power save disable failed %d\n", static_cast<int>(status));
        return false;
    }
    return is_ap() ? start_ap() : start_sta();
}

unsigned long spi_clock_khz() {
    const uint32_t kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1);
    const uint32_t divider = 2U << (LL_SPI_GetBaudRatePrescaler(SPI1) >> SPI_CFG1_MBR_Pos);
    return kernel_hz / divider / 1000;
}
} // namespace

void init() {
    mm_logging_init();
    mmhal_init();
    std::printf("\nmradio halow link test, role %s, spi %lu kHz, built " __DATE__ " " __TIME__ "\n", role_name(),
                spi_clock_khz());
    if (!start_halow() || !link_test::start(local_ip(), peer_ip(), LINK_TEST_FLOOD != 0)) {
        // solid D1 means fail
        set_status_led(true);
    }
}
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
