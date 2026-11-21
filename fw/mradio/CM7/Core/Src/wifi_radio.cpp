#include "wifi_radio.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "stm32h7xx_hal.h"
#include "whd_types.h"
#include "whd_wifi_api.h"
#include "wifi_bt_if.h"

extern "C" {
// main.c keeps the handle but never calls MX_SDMMC1_SD_Init, the pal sets SDMMC1 up itself
extern SD_HandleTypeDef hsd1;
void stm32_cyhal_sdio_irq_handler(void);
// defined in cy_wcm.c but missing from its public header
cy_rslt_t cy_wcm_get_whd_interface(cy_wcm_interface_t interface_type, whd_interface_t *whd_iface);
}

static_assert(PBUF_LINK_ENCAPSULATION_HLEN >= WHD_LINK_HEADER,
              "lwipopts.h must leave room for the WHD headers in front of each frame");

namespace mrover {

namespace {

constexpr uint32_t JOIN_RETRY_MS = 1000;

constexpr osThreadAttr_t JOIN_TASK_ATTRIBUTES = {
    .name = "wifi_join",
    .stack_size = 1024 * 4,
    .priority = osPriorityNormal,
};

// the connection manager's callback carries no argument, so it finds the radio here
WifiRadio *instance = nullptr;

bool copy_c_string(std::span<uint8_t> destination, std::string_view text) {
    if (text.empty() || text.size() >= destination.size()) {
        return false;
    }
    std::memcpy(destination.data(), text.data(), text.size());
    destination[text.size()] = '\0';
    return true;
}

std::optional<cy_wcm_ip_address_t> to_wcm_address(std::string_view text) {
    std::array<char, IP4ADDR_STRLEN_MAX> terminated{};
    if (text.size() >= terminated.size()) {
        return std::nullopt;
    }
    std::memcpy(terminated.data(), text.data(), text.size());
    ip4_addr_t address{};
    if (ip4addr_aton(terminated.data(), &address) == 0) {
        return std::nullopt;
    }
    cy_wcm_ip_address_t wcm_address{};
    wcm_address.version = CY_WCM_IP_VER_V4;
    wcm_address.ip.v4 = ip4_addr_get_u32(&address);
    return wcm_address;
}

bool succeeded(cy_rslt_t result, const char *step) {
    if (result != CY_RSLT_SUCCESS) {
        std::printf("wifi: %s failed 0x%08lx\n", step, static_cast<unsigned long>(result));
        return false;
    }
    return true;
}

cy_wcm_wifi_band_t to_wcm_band(WifiRadio::Band band) {
    return band == WifiRadio::Band::ghz2_4 ? CY_WCM_WIFI_BAND_2_4GHZ : CY_WCM_WIFI_BAND_5GHZ;
}

// cy_network_mw_core makes each new Wi-Fi netif the default, which would pull HaLow's route
class DefaultNetifKeeper {
public:
    DefaultNetifKeeper() {
        LOCK_TCPIP_CORE();
        m_saved = netif_default;
        UNLOCK_TCPIP_CORE();
    }
    ~DefaultNetifKeeper() {
        LOCK_TCPIP_CORE();
        netif_set_default(m_saved);
        UNLOCK_TCPIP_CORE();
    }
    DefaultNetifKeeper(const DefaultNetifKeeper &) = delete;
    DefaultNetifKeeper &operator=(const DefaultNetifKeeper &) = delete;

private:
    netif *m_saved = nullptr;
};

} // namespace

bool WifiRadio::start(const Config &config) {
    if (m_state != State::off || instance != nullptr) {
        return false;
    }
    m_state = State::failed;
    m_role = config.role;

    cy_wcm_ap_credentials_t credentials{};
    credentials.security = CY_WCM_SECURITY_WPA2_AES_PSK;
    const auto local = to_wcm_address(config.local_ip);
    const auto netmask = to_wcm_address(config.netmask);
    const auto gateway = to_wcm_address(config.gateway);
    if (!copy_c_string(credentials.SSID, config.ssid) ||
        !copy_c_string(credentials.password, config.passphrase) || config.passphrase.size() < 8 ||
        !local || !netmask || !gateway) {
        std::printf("wifi: config has an empty, oversized or malformed field\n");
        return false;
    }
    m_ip_settings = {.ip_address = *local, .gateway = *gateway, .netmask = *netmask};

    hsd1.Instance = SDMMC1;
    if (!succeeded(stm32_cypal_wifi_sdio_init(&hsd1), "sdio init")) {
        return false;
    }
    cy_wcm_config_t wcm_config{};
    wcm_config.interface = is_ap() ? CY_WCM_INTERFACE_TYPE_AP : CY_WCM_INTERFACE_TYPE_STA;
    if (!succeeded(cy_wcm_init(&wcm_config), "init")) {
        return false;
    }
    if (!is_ap()) {
        // the connection manager only toggles power save on 43907-class chips, so call WHD directly
        whd_interface_t interface = nullptr;
        if (!succeeded(cy_wcm_get_whd_interface(CY_WCM_INTERFACE_TYPE_STA, &interface),
                       "get whd interface") ||
            !succeeded(whd_wifi_disable_powersave(interface), "disable powersave")) {
            return false;
        }
    }
    instance = this;
    if (!succeeded(cy_wcm_register_event_callback(on_event), "event callback")) {
        return false;
    }

    m_connect_params.ap_credentials = credentials;
    m_connect_params.band = to_wcm_band(config.band);
    m_connect_params.static_ip_settings = &m_ip_settings;
    if (!(is_ap() ? start_ap(config) : start_join_task(config))) {
        return false;
    }
    m_state = State::on;
    return true;
}

bool WifiRadio::start_ap(const Config &config) {
    cy_wcm_ap_config_t ap_config{};
    ap_config.ap_credentials = m_connect_params.ap_credentials;
    ap_config.band = m_connect_params.band;
    ap_config.channel = config.channel;
    ap_config.ip_settings = m_ip_settings;
    const DefaultNetifKeeper keep_halow_default;
    if (!succeeded(cy_wcm_start_ap(&ap_config), "ap start")) {
        return false;
    }
    std::printf("wifi: ap %.*s on %s channel %u\n", static_cast<int>(config.local_ip.size()),
                config.local_ip.data(), config.band == Band::ghz5 ? "5 GHz" : "2.4 GHz",
                static_cast<unsigned>(config.channel));
    return true;
}

bool WifiRadio::start_join_task(const Config &config) {
    if (osThreadNew(join_task, this, &JOIN_TASK_ATTRIBUTES) == nullptr) {
        std::printf("wifi: join task create failed\n");
        return false;
    }
    std::printf("wifi: sta %.*s joining on %s\n", static_cast<int>(config.local_ip.size()),
                config.local_ip.data(), config.band == Band::ghz5 ? "5 GHz" : "2.4 GHz");
    return true;
}

void WifiRadio::join_task(void *argument) {
    auto *const radio = static_cast<WifiRadio *>(argument);
    for (;;) {
        cy_wcm_ip_address_t address{};
        cy_rslt_t result = CY_RSLT_SUCCESS;
        {
            const DefaultNetifKeeper keep_halow_default;
            result = cy_wcm_connect_ap(&radio->m_connect_params, &address);
        }
        if (result == CY_RSLT_SUCCESS) {
            break;
        }
        osDelay(JOIN_RETRY_MS);
    }
    // the connection manager rejoins on its own after a later drop
    osThreadExit();
}

bool WifiRadio::is_peer_up() const { return m_peer_up; }

std::optional<int16_t> WifiRadio::rssi_dbm() const {
    if (is_ap() || !is_peer_up()) {
        return std::nullopt;
    }
    cy_wcm_associated_ap_info_t info{};
    if (cy_wcm_get_associated_ap_info(&info) != CY_RSLT_SUCCESS) {
        return std::nullopt;
    }
    return info.signal_strength;
}

std::optional<uint32_t> WifiRadio::ping(std::string_view peer_ip, uint32_t timeout_ms) const {
    auto address = to_wcm_address(peer_ip);
    if (m_state != State::on || !is_peer_up() || !address) {
        return std::nullopt;
    }
    uint32_t elapsed_ms = 0;
    const cy_wcm_interface_t interface =
        is_ap() ? CY_WCM_INTERFACE_TYPE_AP : CY_WCM_INTERFACE_TYPE_STA;
    if (cy_wcm_ping(interface, &*address, timeout_ms, &elapsed_ms) != CY_RSLT_SUCCESS) {
        return std::nullopt;
    }
    return elapsed_ms;
}

bool WifiRadio::is_ap() const { return m_role == Role::ap; }

void WifiRadio::on_event(cy_wcm_event_t event, cy_wcm_event_data_t * /*event_data*/) {
    if (instance == nullptr) {
        return;
    }
    switch (event) {
    case CY_WCM_EVENT_CONNECTED:
    case CY_WCM_EVENT_RECONNECTED:
    case CY_WCM_EVENT_STA_JOINED_SOFTAP:
        instance->m_peer_up = true;
        std::printf("wifi: peer up\n");
        break;
    case CY_WCM_EVENT_DISCONNECTED:
    case CY_WCM_EVENT_STA_LEFT_SOFTAP:
        instance->m_peer_up = false;
        std::printf("wifi: peer down\n");
        break;
    default:
        break;
    }
}

} // namespace mrover

extern "C" {
void SDMMC1_IRQHandler(void) { stm32_cyhal_sdio_irq_handler(); }
}
