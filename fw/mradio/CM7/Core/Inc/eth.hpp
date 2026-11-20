#pragma once

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

namespace mrover::eth {

// needs the tcpip thread running, false when the MAC gets no RMII clock or finds no PHY
[[nodiscard]] bool start(const ip4_addr_t &ip, const ip4_addr_t &mask);

netif &interface();

} // namespace mrover::eth
