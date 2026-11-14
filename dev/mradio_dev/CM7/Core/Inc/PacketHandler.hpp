#pragma once

#include <cstdint>
#include <cstring>
#include "PacketInfo.hpp"
#include "RingBuffer.hpp"

#define GROUPING_DEADLINE 10 // deadline in ms before a group is flushed

namespace mrover {
    // struct for a slot in the grouping buffer
    struct GroupSlot {
        uint32_t initial_time = 0;
        uint16_t len = 0;
        uint8_t payload[MTU] = {};
    };

    // class for handling grouping and degrouping packets
    class PacketHandler {
    private:
        // grouping buffer per packet type
        GroupSlot group_buf[NUM_PACKET_TYPES];

        // flushes a packet group to tx_buf
        void flushGroup(PacketType type);

    public:
        // default packet handler constructor
        PacketHandler() = default;

        // groups packet, flushes current group to tx_buf if size will exceed MTU
        void groupPacket(const uint8_t* frame, uint16_t len, PacketType type);

        // check if a group has reached its deadline, flush the group if so
        void checkDeadline(PacketType type);

        // degroups container into individual ethernet frames
        template <typename Callback>
        void degroupPackets(const uint8_t* grouped_packet, uint16_t total_len, Callback&& callback) {
            size_t offset = 0;

            while (offset + sizeof(uint16_t) <= total_len) {
                uint16_t frame_len = *(grouped_packet + offset);
                offset += sizeof(frame_len);

                if (offset + frame_len > total_len) {
                    break;
                }

                callback(grouped_packet + offset, frame_len);
                offset += frame_len;
            }
        }
    };
}
