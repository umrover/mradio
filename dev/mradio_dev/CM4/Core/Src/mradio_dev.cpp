#include "PacketInfo.hpp"
#include "main.h"
#include "RingBuffer.hpp"
#include "stm32h7xx_hal.h"
#include <cstdint>

namespace mrover {
    __attribute__((section(".RxRingBuffer"))) RingBuffer rx_ring_buf;
    __attribute__((section(".TxRingBuffer"))) RingBuffer tx_ring_buf;

    extern "C" {
        void postInit() {
            // initialize CM4 stuff
        }

        void cm4_loop() {
            static BufferSlot slot;

            // pop next packet if available
            if (tx_ring_buf.pop(slot)) {
                // transmit packets
                rx_ring_buf.push(slot);
            }
        }
    }
}
