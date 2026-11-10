#include <cstdint>
#include <cstring>
#include "PacketInfo.hpp"
#include "RingBuffer.hpp"
#include "lwip/ip_addr.h"

#define GROUPING_DEADLINE 1000 // deadline in ms before a group is flushed

namespace mrover {
    // struct for a slot in the grouping buffer
    struct GroupSlot {
        uint32_t initial_time = 0;
        uint16_t len = 0;
        uint8_t payload[MTU] = {};
        ip_addr_t remote_ip = {};
	    uint16_t remote_port = 0;
    };

    // class for handling packing and parsing IP packets
    class PacketGrouper {
    private:
        // grouping buffer
        GroupSlot group_buf[NUM_PACKET_TYPES];

        // flushes a packet group to tx_buf
        void flushGroup(PacketType type);

    public:
        // default packet handler constructor
        PacketGrouper() = default;

        // groups packet, flushes current group to tx_buf if size will exceed MTU
        void groupPackets(uint8_t* payload, uint16_t len, PacketType type, const ip_addr_t* addr, uint16_t port);

        // check if a group has reached its deadline, flush the group if so
        void checkDeadline(PacketType type);
    };
}