#include "RingBuffer.hpp"
#include <cstdint>

namespace mrover {
    // initialize ring buffer
    void RingBuffer::init() {
        m_head = 0;
        m_tail = 0;
    }

    // returns true if the buffer is full, false otherwise
    bool RingBuffer::isFull() {
        uint8_t next_head = (m_head + 1) % RB_CAPACITY;
        return next_head == m_tail;
    }

    // returns true if the buffer is empty, false otherwise
    bool RingBuffer::isEmpty() {
        return m_head == m_tail;
    }

    // pushes a packet into the end of the buffer using FIFO order, returns true for success and false for failure
    bool RingBuffer::push(BufferSlot& slot) {
        // ensure there is space in the buffer
        if (slot.len > MTU || isFull()) {
            return false;
        }

        // push new packet into m_buf
        m_buf[m_head] = slot;

        // ensure packet transfer completes before advancing head index
        __DMB();

        // increment head index
        m_head = (m_head + 1) % RB_CAPACITY;

        return true;
    }

    // pops the first packet in the buffer using FIFO order, returns the packet length, returns false on failure
    bool RingBuffer::pop(BufferSlot& slot) {
        // ensure there is a slot to pop
        if (isEmpty()) {
            return false;
        }

        // move packet to caller's packet
        slot = m_buf[m_tail];

        // ensure packet transfer completes before advancing tail index
        __DMB();

        // increment tail index
        m_tail = (m_tail + 1) % RB_CAPACITY;

        return true;
    }
}