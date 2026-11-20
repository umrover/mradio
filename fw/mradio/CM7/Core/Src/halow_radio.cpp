#include "halow_radio.hpp"

#include <cstdio>
#include <cstring>
#include <span>

namespace mrover {

namespace {

bool fits_c_string(std::string_view text, size_t capacity) {
    return !text.empty() && text.size() < capacity;
}

void copy_c_string(std::span<char> destination, std::string_view text) {
    std::memcpy(destination.data(), text.data(), text.size());
    destination[text.size()] = '\0';
}
bool config_valid(const HalowRadio::Config &config) {

    return !config.ssid.empty() && config.ssid.size() <= MMWLAN_SSID_MAXLEN &&
           fits_c_string(config.passphrase, MMWLAN_PASSPHRASE_MAXLEN + 1) &&
           fits_c_string(config.local_ip, MMIPAL_IPADDR_STR_MAXLEN) &&
           fits_c_string(config.netmask, MMIPAL_IPADDR_STR_MAXLEN) &&
           fits_c_string(config.gateway, MMIPAL_IPADDR_STR_MAXLEN) &&
           // TODO: remove after getting BCF
           halow::is_supported(config.sub_band, config.bandwidth);
}

// abstraction over mmwlan_ap_args and mmwlan_sta_args
template <typename LinkArgs>
void fill_credentials(LinkArgs &args, const HalowRadio::Config &config) {
    std::memcpy(args.ssid, config.ssid.data(), config.ssid.size());
    args.ssid_len = static_cast<uint16_t>(config.ssid.size());
    args.security_type = MMWLAN_SAE;
    copy_c_string(args.passphrase, config.passphrase);
    args.passphrase_len = static_cast<uint16_t>(config.passphrase.size());
}

bool succeeded(mmwlan_status status, const char *step) {
    if (status != MMWLAN_SUCCESS) {
        std::printf("halow: %s failed %d\n", step, static_cast<int>(status));
        return false;
    }
    return true;
}

} // namespace

bool HalowRadio::start(const Config &config) {
    if (m_state != State::off) {
        return false;
    }
    if (!config_valid(config)) {
        std::printf("halow: config has an empty or oversized field, or an unsupported band\n");
        return false;
    }
    m_state = State::failed;
    m_sub_band = config.sub_band;
    m_bandwidth = config.bandwidth;
    m_role = config.role;
    if (m_role == Role::ap) {
        mmwlan_ap_args ap_args = MMWLAN_AP_ARGS_INIT;
        fill_credentials(ap_args, config);
        ap_args.sta_status_cb = on_station_change;
        ap_args.sta_status_cb_arg = this;
        m_link_args.emplace<mmwlan_ap_args>(ap_args);
    }
    else {
        mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;
        fill_credentials(sta_args, config);
        m_link_args.emplace<mmwlan_sta_args>(sta_args);
    }

    mmwlan_init();
    if (!succeeded(mmwlan_set_channel_list(&halow::channel_list(m_sub_band, m_bandwidth)),
                   "set channel list")) {
        return false;
    }

    const mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    if (!succeeded(mmwlan_boot(&boot_args), "boot")) {
        return false;
    }

    mmwlan_version version{};
    if (mmwlan_get_version(&version) == MMWLAN_SUCCESS) {
        std::printf("halow: morselib %s fw %s chip %s\n", version.morselib_version,
                    version.morse_fw_version, version.morse_chip_id_string);
    }
    if (config.max_tx_power_dbm != 0 &&
        !succeeded(mmwlan_override_max_tx_power(config.max_tx_power_dbm), "max tx power")) {
        return false;
    }

    mmipal_init_args ip_args = MMIPAL_INIT_ARGS_DEFAULT;
    ip_args.mode = MMIPAL_STATIC;
    ip_args.ip6_mode = MMIPAL_IP6_DISABLED;
    copy_c_string(ip_args.ip_addr, config.local_ip);
    copy_c_string(ip_args.netmask, config.netmask);
    copy_c_string(ip_args.gateway_addr, config.gateway);
    if (mmipal_init(&ip_args) != MMIPAL_SUCCESS) {
        std::printf("halow: ip init failed\n");
        return false;
    }
    mmipal_set_ext_link_status_callback(on_link_status, this);

    // never sleep
    if (!succeeded(mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED), "power save disable") ||
        !enable()) {
        return false;
    }
    m_state = State::on;
    return true;
}

bool HalowRadio::set_band(halow::SubBand sub_band, halow::Bandwidth bandwidth) {
    if (m_state != State::on || !halow::is_supported(sub_band, bandwidth)) {
        return false;
    }
    if (sub_band == m_sub_band && bandwidth == m_bandwidth) {
        return true;
    }

    if (!succeeded(mmwlan_shutdown(), "shutdown")) {
        return false;
    }

    if (!succeeded(mmwlan_set_channel_list(&halow::channel_list(sub_band, bandwidth)),
                   "set channel list")) {
        return false;
    }

    m_sub_band = sub_band;
    m_bandwidth = bandwidth;

    return enable(); // starts module again
}

halow::SubBand HalowRadio::sub_band() const { return m_sub_band; }

halow::Bandwidth HalowRadio::bandwidth() const { return m_bandwidth; }

bool HalowRadio::is_peer_up() const { return m_peer_up; }

std::optional<int32_t> HalowRadio::rssi_dbm() const {
    if (is_ap() || !is_peer_up()) {
        return std::nullopt;
    }
    const int32_t rssi = mmwlan_get_rssi();
    if (rssi == INT32_MIN) {
        return std::nullopt;
    }
    return rssi;
}

bool HalowRadio::is_ap() const { return m_role == Role::ap; }

bool HalowRadio::enable() {
    const mmwlan_s1g_channel &channel = halow::operating_channel(m_sub_band, m_bandwidth);
    mmwlan_status status = MMWLAN_ERROR;
    if (auto *const ap_args = std::get_if<mmwlan_ap_args>(&m_link_args)) {
        ap_args->op_class = static_cast<uint16_t>(channel.global_operating_class);
        ap_args->s1g_chan_num = channel.s1g_chan_num;
        status = mmwlan_ap_enable(ap_args);
    }
    else if (auto *const sta_args = std::get_if<mmwlan_sta_args>(&m_link_args)) {
        status = mmwlan_sta_enable(sta_args, nullptr);
    }
    if (!succeeded(status, is_ap() ? "ap enable" : "sta enable")) {
        return false;
    }
    std::printf("halow: %s on %lu kHz at %u MHz, channel %u\n", is_ap() ? "ap" : "sta",
                static_cast<unsigned long>(channel.centre_freq_hz / 1000),
                static_cast<unsigned>(channel.bw_mhz), static_cast<unsigned>(channel.s1g_chan_num));
    return true;
}

void HalowRadio::on_station_change(const mmwlan_ap_sta_status *status, void *arg) {
    const bool authorized = status->state == MMWLAN_AP_STA_AUTHORIZED;
    std::printf("halow: station aid %u %s\n", static_cast<unsigned>(status->aid),
                authorized ? "authorized" : "not authorized");
    static_cast<HalowRadio *>(arg)->m_peer_up = authorized;
}

void HalowRadio::on_link_status(const mmipal_link_status *status, void *arg) {
    auto *const radio = static_cast<HalowRadio *>(arg);
    const bool up = status->link_state == MMIPAL_LINK_UP;
    std::printf("halow: link %s, ip %s\n", up ? "up" : "down", status->ip_addr);
    if (!radio->is_ap()) {
        radio->m_peer_up = up;
    }
}

} // namespace mrover
