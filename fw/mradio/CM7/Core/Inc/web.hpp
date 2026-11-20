#pragma once

#include "config.hpp"

namespace mrover::web {

// call once the tcpip thread runs, boot_net is used until a net is saved to config
[[nodiscard]] bool init(config::Net boot_net);

} // namespace mrover::web
