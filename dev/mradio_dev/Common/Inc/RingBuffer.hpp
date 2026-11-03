#pragma once
#include <cstdint>
#include "PacketHandler.hpp"

// enum representing buffer slot ownership
enum class Core : uint8_t {
    INGRESS = 0,
    EGRESS = 1
};

// struct for each slot in ring buffer
struct BufferSlot {
    Core owner; // current slot owner
    uint8_t packet[MTU]; // raw eth packet (eth header + payload (payload will be an IP packet))
};

// shared memory ring buffer implementation
class RingBuffer {
private:
    static constexpr uint8_t CAPACITY = 128U; // capacity of the ring buffer

    uint8_t m_head{0}; // head index
    uint8_t m_tail{0}; // tail index
    BufferSlot m_buffer[CAPACITY]; // actual buffer

public:
    // default ring buffer constructor
    RingBuffer();

    // returns true if the buffer is full, false otherwise
    bool isFull();

    // returns true if the buffer is empty, false otherwise
    bool isEmpty();

    // pushes a packet into the end of the buffer using FIFO order, returns true for success and false for failure
    bool push(const uint8_t* packet, uint16_t len);

    // pops the first packet in the buffer using FIFO order, returns the packet length, returns UINT16_MAX on failure
    uint16_t pop(uint8_t* packet);
};