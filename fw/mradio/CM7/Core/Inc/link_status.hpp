#pragma once

#include <cstdint>
#include <optional>

#include "config.hpp"

// written by the radio monitor tasks, read by the status page and config::link_up
namespace mrover::link_status {

struct Snapshot {
    bool up = false;
    std::uint32_t changed_ms = 0; // tick of the last up or down change
    std::optional<std::uint32_t> rtt_ms;
    std::uint32_t heartbeats_sent = 0;
    std::uint32_t heartbeats_acked = 0;
    std::optional<std::int32_t> rssi_dbm;
};

void set_up(config::Link link, bool up);
void record_heartbeats(config::Link link, std::uint32_t sent, std::uint32_t acked,
                       std::optional<std::uint32_t> rtt_ms);
void set_rssi(config::Link link, std::optional<std::int32_t> rssi_dbm);
Snapshot snapshot(config::Link link);

} // namespace mrover::link_status
