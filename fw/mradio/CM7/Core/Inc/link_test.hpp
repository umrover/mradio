#pragma once

#include <cstdint>
#include <string_view>

namespace mrover::link_test {

struct Config {
    std::string_view local_ip;
    std::string_view peer_ip;
    bool is_ap = false;
    bool flood = false;
    // 0 floods as fast as the link takes it
    uint32_t flood_rate_kbps = 0;
};

// receives the peer's flood and prints a report every 5 s, and floods the peer if configured
[[nodiscard]] bool start(const Config &config);
void set_peer_up(bool up);

} // namespace mrover::link_test
