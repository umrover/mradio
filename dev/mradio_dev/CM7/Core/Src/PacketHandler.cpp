#include "PacketHandler.hpp"
#include "PacketInfo.hpp"
#include "stm32h7xx_hal.h"
#include <cstdint>
#include <cstring>

namespace mrover {
    // groups ethernet frame into group_buf slot of its type
    void PacketHandler::groupPacket(const uint8_t* frame, uint16_t len, PacketType type) {
        size_t slot = static_cast<size_t>(type);
        uint16_t subframe_size = sizeof(len) + len;

        // check if packet is valid
        if (subframe_size > MTU) {
            return;
        }

        // check if flush is needed
        if (subframe_size + group_buf[slot].len > MTU) {
            flushGroup(type);
        }

        // check for fresh group
        if (group_buf[slot].len == 0) {
            group_buf[slot].initial_time = HAL_GetTick();
        }

        // write length header
        std::memcpy(group_buf[slot].payload + group_buf[slot].len, &len, sizeof(len));
        group_buf[slot].len += sizeof(len);

        // write frame
        std::memcpy(group_buf[slot].payload + group_buf[slot].len, frame, len);
        group_buf[slot].len += len;
    }

    // check if a group has reached its deadline, flush the group if so
    void PacketHandler::checkDeadline(PacketType type) {
        size_t slot = static_cast<size_t>(type);
        uint32_t current_time = HAL_GetTick();

        if (group_buf[slot].len > 0 && (current_time - group_buf[slot].initial_time >= GROUPING_DEADLINE)) {
            flushGroup(type);
        }
    }

    // flushes a packet group to tx_buf
    void PacketHandler::flushGroup(PacketType type) {
        size_t slot = static_cast<size_t>(type);

        // check slot has data
        if (group_buf[slot].len == 0) {
            return;
        }

        static BufferSlot buf_slot;
        buf_slot.slot_type = SlotType::GROUPED_PACKET;
        buf_slot.len = group_buf[slot].len;

        std::memcpy(buf_slot.packet, group_buf[slot].payload, group_buf[slot].len);
        tx_ring_buf.push(buf_slot);

        // reset group slot
        group_buf[slot].len = 0;
        group_buf[slot].initial_time = 0;
    }
}