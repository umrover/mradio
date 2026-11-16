#pragma once

namespace mrover::link_test {
bool start(const char *local_ip, const char *peer_ip, bool flood);
void set_peer_up(bool up);
} // namespace mrover::link_test
