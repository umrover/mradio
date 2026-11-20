#include "link_status.hpp"

#include <array>
#include <cstddef>

#include "FreeRTOS.h"
#include "stm32h7xx_hal.h"
#include "task.h"

namespace mrover::link_status {

namespace {

std::array<Snapshot, 3> links;

// the monitor tasks and the tcpip thread touch these, so each access holds a short critical
// section, which keeps a snapshot from mixing two updates
class CriticalSection {
public:
    CriticalSection() { taskENTER_CRITICAL(); }
    ~CriticalSection() { taskEXIT_CRITICAL(); }
    CriticalSection(const CriticalSection &) = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;
};

Snapshot &entry(config::Link link) { return links[static_cast<std::size_t>(link)]; }

} // namespace

void set_up(config::Link link, bool up) {
    const std::uint32_t now = HAL_GetTick();
    const CriticalSection lock;
    Snapshot &state = entry(link);
    if (state.up != up) {
        state.up = up;
        state.changed_ms = now;
    }
}

void record_heartbeats(config::Link link, std::uint32_t sent, std::uint32_t acked,
                       std::optional<std::uint32_t> rtt_ms) {
    const CriticalSection lock;
    Snapshot &state = entry(link);
    state.heartbeats_sent = sent;
    state.heartbeats_acked = acked;
    state.rtt_ms = rtt_ms;
}

void set_rssi(config::Link link, std::optional<std::int32_t> rssi_dbm) {
    const CriticalSection lock;
    entry(link).rssi_dbm = rssi_dbm;
}

Snapshot snapshot(config::Link link) {
    const CriticalSection lock;
    return entry(link);
}

} // namespace mrover::link_status

namespace mrover::config {

bool link_up(Link link) { return link_status::snapshot(link).up; }

} // namespace mrover::config
