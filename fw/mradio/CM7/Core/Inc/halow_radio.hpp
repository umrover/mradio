#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "halow_bands.hpp"
#include "mmipal.h"
#include "mmwlan.h"

namespace mrover {

class HalowRadio {
public:
    enum class Role : uint8_t { ap, sta };

    struct Config {
        Role role = Role::sta;
        std::string_view ssid;
        std::string_view passphrase;
        halow::SubBand sub_band = halow::SubBand::mid;
        halow::Bandwidth bandwidth = halow::Bandwidth::mhz8;
        std::string_view local_ip;
        std::string_view netmask = "255.255.255.0";
        std::string_view gateway;
        uint16_t max_tx_power_dbm = 0; // follows 1W FCC limit
    };

    HalowRadio() = default;
    HalowRadio(const HalowRadio &) = delete;
    HalowRadio &operator=(const HalowRadio &) = delete;

    // ignoring return value is catastrophic, use nodiscard
    [[nodiscard]] bool start(const Config &config);
    [[nodiscard]] bool set_band(halow::SubBand sub_band, halow::Bandwidth bandwidth);

    halow::SubBand sub_band() const;
    halow::Bandwidth bandwidth() const;
    bool is_peer_up() const;
    // morselib only measures RSSI when role is station, AP always null
    std::optional<int32_t> rssi_dbm() const;

private:
    enum class State : uint8_t { off, failed, on };

    [[nodiscard]] bool enable();
    bool is_ap() const;
    static void on_station_change(const mmwlan_ap_sta_status *status, void *arg);
    static void on_link_status(const mmipal_link_status *status, void *arg);

    State m_state = State::off;
    halow::SubBand m_sub_band = halow::SubBand::mid;
    halow::Bandwidth m_bandwidth = halow::Bandwidth::mhz8;
    Role m_role = Role::sta;
    std::variant<mmwlan_ap_args, mmwlan_sta_args> m_link_args;
    std::atomic<bool> m_peer_up{false};
};

} // namespace mrover
