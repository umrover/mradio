#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ArduinoJson.hpp"

namespace mrover::config {

enum class Role : std::uint8_t { base, rover };
enum class Link : std::uint8_t { halow, wifi_2_4, wifi_5 };

struct Net {
    std::uint32_t ip = 0;
    std::uint32_t mask = 0;
};

// halow and wifi are separate structs because of how center_khz is represented
// a wifi link using a certain channel with bandwidth > 20mhz may not be centered on that channel
// while 900mhz doesn't care and just has center frequency and bandwidth
struct Halow {
    std::int32_t center_khz = 0;
    std::int32_t bw_mhz = 0;
    std::int32_t tx_power_dbm = 0;
    std::optional<std::int32_t> mcs;
};

struct Wifi {
    std::int32_t channel = 0;
    std::int32_t bw_mhz = 0;
    std::int32_t tx_power_dbm = 0;
};

struct Config {
    Role role = Role::base;
    Net net;
    bool passphrase_set = false;
    Halow halow;
    Wifi wifi_2_4;
    Wifi wifi_5;
};

struct ApiError {
    int status = 500;
    std::string code;
    std::string field;
    std::string reason;
};

void init(Net boot_net);
const Config& current();
Role boot_role();
bool link_up(Link link);

[[nodiscard]] std::optional<ApiError> apply_patch(std::string_view body);

void write_json(ArduinoJson::JsonVariant out, const Config& config);
std::string format_ip(std::uint32_t address);
std::string_view role_name(Role role);

} // namespace mrover::config
