#include "config.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdio>
#include <initializer_list>
#include <utility>

namespace mrover::config {
namespace {

using ArduinoJson::JsonObjectConst;
using ArduinoJson::JsonVariantConst;

// TODO: set actual bounds
constexpr std::int32_t TX_POWER_MIN_DBM = 0;
constexpr std::int32_t TX_POWER_MAX_DBM = 30;

// TODO: CONFIG WRITES DONT WRITE TO FLASH YET
Config current_config{
    .role = Role::base,
    .net = {},
    .passphrase_set = false,
    .halow = {.sub_band = SubBand::mid, .bw_mhz = 8, .tx_power_dbm = 20, .mcs = std::nullopt},
    .wifi_2_4 = {.channel = 6, .bw_mhz = 20, .tx_power_dbm = 20},
    .wifi_5 = {.channel = 149, .bw_mhz = 80, .tx_power_dbm = 20},
};
std::string current_passphrase;
Role booted_role = Role::base;

std::string field_path(std::string_view parent, std::string_view key) {
    std::string path{parent};
    if (!path.empty()) {
        path += '.';
    }
    path += key;
    return path;
}

std::optional<ApiError> read_int(JsonVariantConst value, std::string_view path, std::int32_t &out) {
    if (value.is<std::int32_t>()) {
        out = value.as<std::int32_t>();
        return std::nullopt;
    }
    if (value.is<std::int64_t>() || value.is<std::uint64_t>()) {
        return make_error(400, "out_of_range", path, "integer does not fit in 32 bits");
    }
    return make_error(400, "bad_type", path, "expected an integer");
}

std::optional<ApiError> read_bool(JsonVariantConst value, std::string_view path, bool &out) {
    if (!value.is<bool>()) {
        return make_error(400, "bad_type", path, "expected a boolean");
    }
    out = value.as<bool>();
    return std::nullopt;
}

// only accept printable ASCII
std::optional<ApiError> read_string(JsonVariantConst value, std::string_view path,
                                    std::string_view &out) {
    if (!value.is<std::string_view>()) {
        return make_error(400, "bad_type", path, "expected a string");
    }
    const auto text = value.as<std::string_view>();
    if (!std::all_of(text.begin(), text.end(), [](char c) { return c >= 0x20 && c <= 0x7e; })) {
        return make_error(400, "out_of_range", path, "string not printable ASCII");
    }
    out = text;
    return std::nullopt;
}

std::optional<std::uint32_t> parse_ipv4(std::string_view text) {
    std::uint32_t address = 0;
    const char *cursor = text.data();
    const char *const end = text.data() + text.size();
    for (int octet_index = 0; octet_index < 4; ++octet_index) {
        if (octet_index > 0) {
            if (cursor == end || *cursor != '.') {
                return std::nullopt;
            }
            ++cursor;
        }
        if (cursor == end || *cursor < '0' || *cursor > '9') {
            return std::nullopt;
        }
        unsigned octet = 0;
        const auto [octet_end, error] = std::from_chars(cursor, end, octet);
        if (error != std::errc{} || octet > 255 || octet_end - cursor > 3) {
            return std::nullopt;
        }
        address = (address << 8) | octet;
        cursor = octet_end;
    }
    if (cursor != end) {
        return std::nullopt;
    }
    return address;
}

std::optional<ApiError> read_ipv4(JsonVariantConst value, std::string_view path,
                                  std::uint32_t &out) {
    std::string_view text;
    if (auto error = read_string(value, path, text)) {
        return error;
    }
    const auto address = parse_ipv4(text);
    if (!address) {
        return make_error(400, "out_of_range", path, "expected a dotted IPv4 address");
    }
    out = *address;
    return std::nullopt;
}

template <typename FieldReader>
std::optional<ApiError> read_object(JsonVariantConst value, std::string_view path,
                                    FieldReader &&read_field) {
    if (!value.is<JsonObjectConst>()) {
        return make_error(400, "bad_type", path, "expected an object");
    }
    for (const ArduinoJson::JsonPairConst member : value.as<JsonObjectConst>()) {
        const std::string_view name{member.key().c_str(), member.key().size()};
        if (auto error = read_field(name, member.value(), field_path(path, name))) {
            return error;
        }
    }
    return std::nullopt;
}

std::optional<ApiError> read_sub_band(JsonVariantConst value, std::string_view path, SubBand &out) {
    std::string_view text;
    if (auto error = read_string(value, path, text)) {
        return error;
    }
    for (const SubBand sub_band : {SubBand::low, SubBand::mid, SubBand::high}) {
        if (text == sub_band_name(sub_band)) {
            out = sub_band;
            return std::nullopt;
        }
    }
    return make_error(400, "out_of_range", path, "expected \"low\", \"mid\" or \"high\"");
}

ApiError unknown_field(std::string_view path) { return make_error(400, "unknown_field", path, ""); }

std::optional<ApiError> read_patch(JsonVariantConst root, Config &candidate,
                                   std::optional<std::string_view> &new_passphrase) {
    const auto read_net = [&](std::string_view name, JsonVariantConst value,
                              std::string_view path) -> std::optional<ApiError> {
        if (name == "ip") {
            return read_ipv4(value, path, candidate.net.ip);
        }
        if (name == "mask") {
            return read_ipv4(value, path, candidate.net.mask);
        }
        return unknown_field(path);
    };

    const auto read_link = [&](std::string_view name, JsonVariantConst value,
                               std::string_view path) -> std::optional<ApiError> {
        if (name == "passphrase") {
            std::string_view text;
            if (auto error = read_string(value, path, text)) {
                return error;
            }
            new_passphrase = text;
            return std::nullopt;
        }
        if (name == "passphrase_set") {
            bool flag = false;
            if (auto error = read_bool(value, path, flag)) {
                return error;
            }
            if (flag != candidate.passphrase_set) {
                return make_error(400, "out_of_range", path, "read only");
            }
            return std::nullopt;
        }
        return unknown_field(path);
    };

    const auto read_halow = [&](std::string_view name, JsonVariantConst value,
                                std::string_view path) -> std::optional<ApiError> {
        if (name == "sub_band") {
            return read_sub_band(value, path, candidate.halow.sub_band);
        }
        if (name == "bw_mhz") {
            return read_int(value, path, candidate.halow.bw_mhz);
        }
        if (name == "tx_power_dbm") {
            return read_int(value, path, candidate.halow.tx_power_dbm);
        }
        if (name == "mcs") {
            if (value.isNull()) {
                candidate.halow.mcs = std::nullopt;
                return std::nullopt;
            }
            std::int32_t mcs = 0;
            if (auto error = read_int(value, path, mcs)) {
                return error;
            }
            candidate.halow.mcs = mcs;
            return std::nullopt;
        }
        return unknown_field(path);
    };

    const auto wifi_reader = [](Wifi &wifi) {
        return [&wifi](std::string_view name, JsonVariantConst value,
                       std::string_view path) -> std::optional<ApiError> {
            if (name == "channel") {
                return read_int(value, path, wifi.channel);
            }
            if (name == "bw_mhz") {
                return read_int(value, path, wifi.bw_mhz);
            }
            if (name == "tx_power_dbm") {
                return read_int(value, path, wifi.tx_power_dbm);
            }
            return unknown_field(path);
        };
    };

    const auto read_root = [&](std::string_view name, JsonVariantConst value,
                              std::string_view path) -> std::optional<ApiError> {
        if (name == "role") {
            std::string_view text;
            if (auto error = read_string(value, path, text)) {
                return error;
            }
            if (text == "base") {
                candidate.role = Role::base;
            } else if (text == "rover") {
                candidate.role = Role::rover;
            } else {
                return make_error(400, "out_of_range", path, "expected \"base\" or \"rover\"");
            }
            return std::nullopt;
        }
        if (name == "net") {
            return read_object(value, path, read_net);
        }
        if (name == "link") {
            return read_object(value, path, read_link);
        }
        if (name == "halow") {
            return read_object(value, path, read_halow);
        }
        if (name == "wifi_2_4") {
            return read_object(value, path, wifi_reader(candidate.wifi_2_4));
        }
        if (name == "wifi_5") {
            return read_object(value, path, wifi_reader(candidate.wifi_5));
        }
        return unknown_field(path);
    };

    return read_object(root, "", read_root);
}

bool contains(std::initializer_list<std::int32_t> values, std::int32_t value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool tx_power_in_range(std::int32_t dbm) {
    return dbm >= TX_POWER_MIN_DBM && dbm <= TX_POWER_MAX_DBM;
}

std::optional<ApiError> validate_net(const Net &net) {
    const std::uint32_t host_bits = ~net.mask;
    const bool contiguous = (host_bits & (host_bits + 1)) == 0;
    const int prefix_length = std::popcount(net.mask);
    if (!contiguous || prefix_length < 8 || prefix_length > 30) {
        return make_error(400, "out_of_range", "net.mask", "expected a contiguous /8 to /30 mask");
    }
    const std::uint32_t first_octet = net.ip >> 24;
    const std::uint32_t host = net.ip & host_bits;
    // 0.x, loopback and multicast or reserved ranges are never valid host addresses
    if (first_octet == 0 || first_octet == 127 || first_octet >= 224 || host == 0 ||
        host == host_bits) {
        return make_error(400, "out_of_range", "net.ip", "not a usable host address for the mask");
    }
    return std::nullopt;
}

std::optional<ApiError> validate_halow(const Halow &halow, const Halow &previous) {
    if (halow.bw_mhz != previous.bw_mhz && !contains({1, 2, 4, 8}, halow.bw_mhz)) {
        return make_error(400, "out_of_range", "halow.bw_mhz", "expected 1, 2, 4 or 8");
    }
    if (halow.tx_power_dbm != previous.tx_power_dbm && !tx_power_in_range(halow.tx_power_dbm)) {
        return make_error(400, "out_of_range", "halow.tx_power_dbm", "expected 0 to 30");
    }
    if (halow.mcs != previous.mcs && halow.mcs && (*halow.mcs < 0 || *halow.mcs > 7)) {
        return make_error(400, "out_of_range", "halow.mcs", "expected 0 to 7 or null");
    }
    return std::nullopt;
}

bool retuned(const Halow &halow, const Halow &previous) {
    return halow.sub_band != previous.sub_band || halow.bw_mhz != previous.bw_mhz;
}

bool retuned(const Wifi &wifi, const Wifi &previous) {
    return wifi.channel != previous.channel || wifi.bw_mhz != previous.bw_mhz;
}

std::optional<ApiError> validate_wifi(std::string_view section, const Wifi &wifi,
                                      const Wifi &previous, bool channel_valid,
                                      std::initializer_list<std::int32_t> widths_mhz) {
    if (retuned(wifi, previous)) {
        if (!channel_valid) {
            return make_error(400, "out_of_range", field_path(section, "channel"),
                              "not a US channel for the band");
        }
        if (!contains(widths_mhz, wifi.bw_mhz)) {
            return make_error(400, "out_of_range", field_path(section, "bw_mhz"),
                              "width not available in the band");
        }
    }
    if (wifi.tx_power_dbm != previous.tx_power_dbm && !tx_power_in_range(wifi.tx_power_dbm)) {
        return make_error(400, "out_of_range", field_path(section, "tx_power_dbm"),
                          "expected 0 to 30");
    }
    return std::nullopt;
}

std::string retuned_field(const Halow &halow, const Halow &previous) {
    return halow.sub_band != previous.sub_band ? "halow.sub_band" : "halow.bw_mhz";
}

std::string retuned_field(std::string_view section, const Wifi &wifi, const Wifi &previous) {
    return field_path(section, wifi.channel != previous.channel ? "channel" : "bw_mhz");
}

bool other_link_up(Link retuned_link) {
    const auto links = {Link::halow, Link::wifi_2_4, Link::wifi_5};
    return std::any_of(links.begin(), links.end(),
                       [retuned_link](Link link) { return link != retuned_link && link_up(link); });
}

// TODO: not implemented
bool peer_reachable() { return false; }

} // namespace

ApiError make_error(int status, std::string_view code, std::string_view field,
                    std::string_view reason) {
    return ApiError{.status = status,
                    .code = std::string{code},
                    .field = std::string{field},
                    .reason = std::string{reason}};
}

void init(Net boot_net) {
    current_config.net = boot_net;
    booted_role = current_config.role;
}

const Config &current() { return current_config; }

Role boot_role() { return booted_role; }

// TODO: not implemented
bool link_up(Link /*link*/) { return false; }

std::optional<ApiError> apply_patch(std::string_view body) {
    ArduinoJson::JsonDocument patch;
    const ArduinoJson::DeserializationError parse_error = deserializeJson(patch, body);
    if (parse_error == ArduinoJson::DeserializationError::NoMemory) {
        return make_error(500, "internal", "", "out of memory");
    }
    if (parse_error || !patch.is<JsonObjectConst>()) {
        return make_error(400, "bad_json", "", "body must be one JSON object");
    }

    const Config &previous = current_config;
    Config candidate = previous;
    std::optional<std::string_view> new_passphrase;
    if (auto error = read_patch(patch.as<JsonVariantConst>(), candidate, new_passphrase)) {
        return error;
    }

    const bool net_changed =
        candidate.net.ip != previous.net.ip || candidate.net.mask != previous.net.mask;
    if (net_changed) {
        if (auto error = validate_net(candidate.net)) {
            return error;
        }
    }
    if (auto error = validate_halow(candidate.halow, previous.halow)) {
        return error;
    }
    const Wifi &wifi_2_4 = candidate.wifi_2_4;
    if (auto error = validate_wifi("wifi_2_4", wifi_2_4, previous.wifi_2_4,
                                   wifi_2_4.channel >= 1 && wifi_2_4.channel <= 11, {20, 40})) {
        return error;
    }
    const Wifi &wifi_5 = candidate.wifi_5;
    if (auto error =
            validate_wifi("wifi_5", wifi_5, previous.wifi_5,
                          contains({36,  40,  44,  48,  52,  56,  60,  64,  100, 104, 108, 112, 116,
                                    120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165},
                                   wifi_5.channel),
                          {20, 40, 80})) {
        return error;
    }
    const bool passphrase_changed = new_passphrase && *new_passphrase != current_passphrase;
    if (passphrase_changed && (new_passphrase->size() < 8 || new_passphrase->size() > 63)) {
        return make_error(400, "out_of_range", "link.passphrase", "expected 8 to 63 characters");
    }

    const bool halow_retuned = retuned(candidate.halow, previous.halow);
    const bool wifi_2_4_retuned = retuned(wifi_2_4, previous.wifi_2_4);
    const bool wifi_5_retuned = retuned(wifi_5, previous.wifi_5);
    if (int{halow_retuned} + int{wifi_2_4_retuned} + int{wifi_5_retuned} > 1) {
        return make_error(400, "both_links_changed", "", "retune one link at a time");
    }
    std::optional<Link> retuned_link;
    std::string retuned_path;
    if (halow_retuned) {
        retuned_link = Link::halow;
        retuned_path = retuned_field(candidate.halow, previous.halow);
    } else if (wifi_2_4_retuned) {
        retuned_link = Link::wifi_2_4;
        retuned_path = retuned_field("wifi_2_4", wifi_2_4, previous.wifi_2_4);
    } else if (wifi_5_retuned) {
        retuned_link = Link::wifi_5;
        retuned_path = retuned_field("wifi_5", wifi_5, previous.wifi_5);
    }
    if (retuned_link && !other_link_up(*retuned_link)) {
        return make_error(409, "other_link_down", retuned_path, "no other link is up");
    }
    if ((retuned_link || passphrase_changed) && !peer_reachable()) {
        return make_error(502, "peer_unreachable", retuned_link ? retuned_path : "link.passphrase",
                          "no peer protocol yet");
    }

    if (passphrase_changed) {
        current_passphrase = *new_passphrase;
        candidate.passphrase_set = true;
    }
    current_config = candidate;
    return std::nullopt;
}

std::string format_ipv4(std::uint32_t address) {
    std::string text(15, '\0');
    const int length = std::snprintf(
        text.data(), text.size() + 1, "%u.%u.%u.%u", static_cast<unsigned>((address >> 24) & 0xff),
        static_cast<unsigned>((address >> 16) & 0xff), static_cast<unsigned>((address >> 8) & 0xff),
        static_cast<unsigned>(address & 0xff));
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::string_view role_name(Role role) { return role == Role::base ? "base" : "rover"; }

std::string_view sub_band_name(SubBand sub_band) {
    switch (sub_band) {
    case SubBand::low:
        return "low";
    case SubBand::mid:
        return "mid";
    case SubBand::high:
        return "high";
    }
    std::unreachable();
}

void write_json(ArduinoJson::JsonVariant out, const Config &config) {
    out["role"] = role_name(config.role);
    out["net"]["ip"] = format_ipv4(config.net.ip);
    out["net"]["mask"] = format_ipv4(config.net.mask);
    out["link"]["passphrase_set"] = config.passphrase_set;
    out["halow"]["sub_band"] = sub_band_name(config.halow.sub_band);
    out["halow"]["bw_mhz"] = config.halow.bw_mhz;
    out["halow"]["tx_power_dbm"] = config.halow.tx_power_dbm;
    if (config.halow.mcs) {
        out["halow"]["mcs"] = *config.halow.mcs;
    } else {
        out["halow"]["mcs"] = nullptr;
    }
    for (const auto &[section, wifi] :
         {std::pair{"wifi_2_4", config.wifi_2_4}, std::pair{"wifi_5", config.wifi_5}}) {
        out[section]["channel"] = wifi.channel;
        out[section]["bw_mhz"] = wifi.bw_mhz;
        out[section]["tx_power_dbm"] = wifi.tx_power_dbm;
    }
}

} // namespace mrover::config
