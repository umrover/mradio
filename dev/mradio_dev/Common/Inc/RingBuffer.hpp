#pragma once
#include <cstdint>
#include <cstring>
#include "PacketInfo.hpp"
#include "stm32h7xx_hal.h"

#define RB_CAPACITY 16 // capacity of the ring buffer

namespace mrover {
    // struct for each slot in ring buffer
    struct BufferSlot {
        uint8_t packet[MTU]; // raw eth packet (eth header + payload (payload will be an IP packet))
        uint16_t len; // length of the packet
        ProtocolType protocol_type; // protocol type for packet (udp or tcp)
        uint32_t ip_addr; // ip address to return packet to
        uint16_t port; // port to return packet to
        void* tpcb; // tcp_pcb for tcp connections
    };

    // shared memory ring buffer implementation
    class RingBuffer {
    private:
        volatile uint8_t m_head; // head index
        volatile uint8_t m_tail; // tail index
        BufferSlot m_buf[RB_CAPACITY]; // actual buffer

    public:
        // default ring buffer constructor
        RingBuffer() = default;

        // initialize ring buffer
        void init();

        // returns true if the buffer is full, false otherwise
        bool isFull();

        // returns true if the buffer is empty, false otherwise
        bool isEmpty();

        // pushes a packet into the end of the buffer using FIFO order, returns true for success and false for failure
        bool push(BufferSlot& slot);

        // pops the first packet in the buffer using FIFO order, returns the packet length, returns false on failure
        bool pop(BufferSlot& slot);
    };

    extern RingBuffer tx_ring_buf;
    extern RingBuffer rx_ring_buf;
}