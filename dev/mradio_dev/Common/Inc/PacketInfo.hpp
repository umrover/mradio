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

    // enum representing the slot type for a packet
    enum class SlotType : uint8_t {
        SINGLE_FRAME = 0, // single ethernet frame
        GROUPED_PACKET = 1 // grouped ethernet frames
    };
}