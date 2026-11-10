#pragma once

#include <cstdint>

#define MTU 1536 // ETH header + IP payload + VLAN + CRC
#define NUM_PACKET_TYPES 3 // number of unique packet types

namespace mrover {
    // enum representing packet type
    enum class PacketType : uint8_t {
        CAMERA = 0,
        TELEMETRY = 1,
        COMMAND = 2
    };

    // enum representing the protocol type for a packet
    enum class ProtocolType : uint8_t {
        UDP = 0,
        TCP = 1
	};

    // packet sub-header for grouping
    struct PacketSubHeader {
        uint32_t ip;
        uint16_t port;
        uint16_t len;
    };
}