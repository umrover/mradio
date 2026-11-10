#include "PacketGrouper.hpp"
#include "PacketInfo.hpp"
#include "stm32h7xx_hal.h"
#include <cstdint>

namespace mrover {
    // groups packet into group_buf slot of its type
    void PacketGrouper::groupPackets(uint8_t* payload, uint16_t len, PacketType type, const ip_addr_t* addr, uint16_t port) {
        size_t slot = static_cast<size_t>(type);

        // check if packet is valid
        uint16_t packet_size = sizeof(PacketSubHeader) + len;
        if (packet_size > MTU) {
            return;
        }

        // check if flush is needed
        if (packet_size + group_buf[slot].len > MTU) {
            flushGroup(type);
        }

        // check for fresh group
        if (group_buf[slot].len == 0) {
            group_buf[slot].initial_time = HAL_GetTick();
        }

        // format sub-header
        PacketSubHeader sub_header;
        sub_header.ip = ip_addr_get_ip4_u32(addr);
        sub_header.port = port;
        sub_header.len = len;

        // write length header
        std::memcpy(group_buf[slot].payload + group_buf[slot].len, &sub_header, sizeof(sub_header));
        group_buf[slot].len += sizeof(PacketSubHeader);

        // write packet payload
        std::memcpy(group_buf[slot].payload + group_buf[slot].len, payload, len);
        group_buf[slot].len += len;
    }

    // check if a group has reached its deadline, flush the group if so
    void PacketGrouper::checkDeadline(PacketType type) {
        size_t slot = static_cast<size_t>(type);
        uint32_t current_time = HAL_GetTick();

        if (group_buf[slot].len > 0 && (current_time - group_buf[slot].initial_time >= GROUPING_DEADLINE)) {
            flushGroup(type);
        }
    }

    // flushes a packet group to tx_buf
    void PacketGrouper::flushGroup(PacketType type) {
        size_t slot = static_cast<size_t>(type);

        // check slot exists
        if (group_buf[slot].len == 0) {
            return;
        }

        // flush grouped slot to tx_buf
        static BufferSlot buf_slot;
        buf_slot.protocol_type = ProtocolType::UDP;
        buf_slot.len = group_buf[slot].len;
        buf_slot.tpcb = nullptr;

        std::memcpy(buf_slot.packet, group_buf[slot].payload, group_buf[slot].len);
        tx_ring_buf.push(buf_slot);

        // reset group slot
        group_buf[slot].len = 0;
        group_buf[slot].initial_time = 0;
    }
}