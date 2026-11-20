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
// competition sub-bands: 902 to 910, 911 to 919, 920 to 928 MHz
enum class SubBand : std::uint8_t { low, mid, high };

struct Net {
    std::uint32_t ip = 0;
    std::uint32_t mask = 0;
};

struct Halow {
    SubBand sub_band = SubBand::mid;
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

ApiError make_error(int status, std::string_view code, std::string_view field,
                    std::string_view reason);

void init(Net boot_net);
const Config &current();
Role boot_role();
bool link_up(Link link);

[[nodiscard]] std::optional<ApiError> apply_patch(std::string_view body);

void write_json(ArduinoJson::JsonVariant out, const Config &config);
std::string format_ipv4(std::uint32_t address);
std::string_view role_name(Role role);
std::string_view sub_band_name(SubBand sub_band);

} // namespace mrover::config
