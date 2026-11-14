#pragma once
#include <cstdint>
#include <cstring>
#include "PacketInfo.hpp"
#include "stm32h7xx_hal.h"

#define RB_CAPACITY 16 // capacity of the ring buffer

namespace mrover {
    // struct for each slot in ring buffer
    struct BufferSlot {
        uint8_t packet[MTU]; // raw packet
        uint16_t len; // length of the packet
        SlotType slot_type; // single frame or grouped frames
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
        bool push(const BufferSlot& slot);

        // pops the first packet in the buffer using FIFO order, returns false on failure
        bool pop(BufferSlot& slot);
    };

    extern RingBuffer tx_ring_buf;
    extern RingBuffer rx_ring_buf;
}