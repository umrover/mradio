#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>

#include "cmsis_os2.h"
#include "cy_wcm.h"

namespace mrover {

// Ezurio Sona IF573 over SDIO, through Infineon's connection manager
class WifiRadio {
public:
    enum class Role : uint8_t { ap, sta };
    enum class Band : uint8_t { ghz2_4, ghz5 };

    struct Config {
        Role role = Role::sta;
        std::string_view ssid;
        std::string_view passphrase;
        Band band = Band::ghz5;
        uint8_t channel = 36; // the AP picks the channel, the station follows it
        std::string_view local_ip;
        std::string_view netmask = "255.255.255.0";
        std::string_view gateway;
    };

    WifiRadio() = default;
    WifiRadio(const WifiRadio &) = delete;
    WifiRadio &operator=(const WifiRadio &) = delete;

    // needs the tcpip thread running, a station keeps joining in the background after this
    [[nodiscard]] bool start(const Config &config);

    bool is_peer_up() const;
    // only a station sees one AP to measure, so the AP always returns null
    std::optional<int16_t> rssi_dbm() const;
    // round trip in ms, null on timeout or with no peer
    std::optional<uint32_t> ping(std::string_view peer_ip, uint32_t timeout_ms) const;

private:
    enum class State : uint8_t { off, failed, on };

    [[nodiscard]] bool start_ap(const Config &config);
    [[nodiscard]] bool start_join_task(const Config &config);
    [[noreturn]] static void join_task(void *argument);
    static void on_event(cy_wcm_event_t event, cy_wcm_event_data_t *event_data);
    bool is_ap() const;

    State m_state = State::off;
    Role m_role = Role::sta;
    cy_wcm_ip_setting_t m_ip_settings{};
    cy_wcm_connect_params_t m_connect_params{};
    std::atomic<bool> m_peer_up{false};
};

} // namespace mrover
