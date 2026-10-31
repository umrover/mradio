// Host check for the config patch rules:
// g++ -std=c++23 -Wall -Wextra -Wpedantic -I CM7/Core/Inc CM7/test/config_test.cpp CM7/Core/Src/config.cpp -o /tmp/config_test && /tmp/config_test

#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

#include "config.hpp"

using mrover::config::apply_patch;

namespace {

void expect_error(std::string_view body, int status, std::string_view code,
                  std::string_view field) {
    const auto error = apply_patch(body);
    if (!error || error->status != status || error->code != code ||
        error->field != field) {
        std::printf("FAIL %.*s -> %s\n", static_cast<int>(body.size()), body.data(),
                    error ? error->field.c_str() : "accepted");
        assert(false);
    }
}

std::string config_json() {
    ArduinoJson::JsonDocument document;
    mrover::config::write_json(document.to<ArduinoJson::JsonVariant>(), mrover::config::current());
    std::string text;
    serializeJson(document, text);
    return text;
}

} // namespace

int main() {
    mrover::config::init({.ip = 0xC0A8010A, .mask = 0xFFFFFF00});

    assert(config_json() ==
           R"({"role":"base","net":{"ip":"192.168.1.10","mask":"255.255.255.0"},)"
           R"("link":{"passphrase_set":false},)"
           R"("halow":{"center_khz":915000,"bw_mhz":1,"tx_power_dbm":20,"mcs":null},)"
           R"("wifi_2_4":{"channel":6,"bw_mhz":20,"tx_power_dbm":20},)"
           R"("wifi_5":{"channel":149,"bw_mhz":80,"tx_power_dbm":20}})");

    expect_error("", 400, "bad_json", "");
    expect_error("[]", 400, "bad_json", "");
    expect_error(R"({"role":"base",)", 400, "bad_json", "");
    expect_error(R"({"colour":1})", 400, "unknown_field", "colour");
    expect_error(R"({"halow":{"power":1}})", 400, "unknown_field", "halow.power");
    const std::string long_key(60, 'k');
    expect_error("{\"" + long_key + "\":1}", 400, "unknown_field", long_key);
    expect_error(R"({"halow":1})", 400, "bad_type", "halow");
    expect_error(R"({"halow":{"tx_power_dbm":"20"}})", 400, "bad_type", "halow.tx_power_dbm");
    expect_error(R"({"halow":{"tx_power_dbm":1.5}})", 400, "bad_type", "halow.tx_power_dbm");
    expect_error(R"({"halow":{"mcs":8}})", 400, "out_of_range", "halow.mcs");
    expect_error(R"({"halow":{"tx_power_dbm":99999999999}})", 400, "out_of_range",
                 "halow.tx_power_dbm");
    expect_error(R"({"role":"admin"})", 400, "out_of_range", "role");
    expect_error(R"({"net":{"mask":"255.0.255.0"}})", 400, "out_of_range", "net.mask");
    expect_error(R"({"net":{"ip":"192.168.1.255"}})", 400, "out_of_range", "net.ip");
    expect_error(R"({"net":{"ip":"192.168.1"}})", 400, "out_of_range", "net.ip");
    expect_error(R"({"link":{"passphrase_set":true}})", 400, "out_of_range", "link.passphrase_set");
    expect_error(R"({"link":{"passphrase":"short"}})", 400, "out_of_range", "link.passphrase");
    expect_error(R"({"link":{"passphrase":"Łukasz12"}})", 400, "out_of_range",
                 "link.passphrase");
    expect_error(R"({"halow":{"center_khz":915250}})", 400, "out_of_range", "halow.center_khz");
    expect_error(R"({"halow":{"center_khz":927800,"bw_mhz":8}})", 400, "out_of_range",
                 "halow.center_khz");
    expect_error(R"({"halow":{"center_khz":-2147483500}})", 400, "out_of_range", "halow.center_khz");
    expect_error(R"({"halow":{"center_khz":2147483500,"bw_mhz":8}})", 400, "out_of_range",
                 "halow.center_khz");
    expect_error(R"({"wifi":{"channel":1}})", 400, "unknown_field", "wifi");
    expect_error(R"({"wifi_2_4":{"band":"5"}})", 400, "unknown_field", "wifi_2_4.band");
    expect_error(R"({"wifi_2_4":{"channel":36}})", 400, "out_of_range", "wifi_2_4.channel");
    expect_error(R"({"wifi_5":{"channel":6}})", 400, "out_of_range", "wifi_5.channel");
    expect_error(R"({"wifi_2_4":{"bw_mhz":80}})", 400, "out_of_range", "wifi_2_4.bw_mhz");
    expect_error(R"({"wifi_5":{"tx_power_dbm":31}})", 400, "out_of_range", "wifi_5.tx_power_dbm");
    expect_error(R"({"halow":{"bw_mhz":2},"wifi_2_4":{"channel":1}})", 400, "both_links_changed",
                 "");
    expect_error(R"({"wifi_2_4":{"channel":1},"wifi_5":{"channel":36}})", 400,
                 "both_links_changed", "");
    expect_error(R"({"halow":{"bw_mhz":2}})", 409, "other_link_down", "halow.bw_mhz");
    expect_error(R"({"wifi_2_4":{"channel":1}})", 409, "other_link_down", "wifi_2_4.channel");
    expect_error(R"({"wifi_5":{"bw_mhz":40}})", 409, "other_link_down", "wifi_5.bw_mhz");
    expect_error(R"({"link":{"passphrase":"long enough"}})", 502, "peer_unreachable",
                 "link.passphrase");

    // A rejected patch must leave every field untouched, including ones that were valid
    expect_error(R"({"halow":{"tx_power_dbm":10,"mcs":9}})", 400, "out_of_range", "halow.mcs");
    assert(mrover::config::current().halow.tx_power_dbm == 20);

    // Shared fields sent with their current value are not changes
    assert(!apply_patch(R"( {"role":"rover","net":{"ip":"10.0.0.2","mask":"255.0.0.0"},)"
                        R"("halow":{"center_khz":915000,"tx_power_dbm":12,"mcs":3},)"
                        R"("wifi_2_4":{"channel":6,"tx_power_dbm":5},"wifi_5":{"tx_power_dbm":7},)"
                        R"("link":{"passphrase_set":false}} )"));
    const auto& current = mrover::config::current();
    assert(current.role == mrover::config::Role::rover);
    assert(current.net.ip == 0x0A000002 && current.net.mask == 0xFF000000);
    assert(current.halow.tx_power_dbm == 12 && current.halow.mcs == 3);
    assert(current.wifi_2_4.tx_power_dbm == 5 && current.wifi_5.tx_power_dbm == 7);
    assert(mrover::config::boot_role() == mrover::config::Role::base);

    assert(!apply_patch(R"({"halow":{"mcs":null}})"));
    assert(!current.halow.mcs);

    std::puts("config_test ok");
    return 0;
}
