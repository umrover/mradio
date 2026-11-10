#include "PacketInfo.hpp"
#include "main.h"
#include "RingBuffer.hpp"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"
#include <cstdint>

namespace mrover {
    __attribute__((section(".RxRingBuffer"))) RingBuffer rx_ring_buf;
    __attribute__((section(".TxRingBuffer"))) RingBuffer tx_ring_buf;

    extern "C" {
        void postInit() {
            static BufferSlot slot;

            while (true) {
                if (tx_ring_buf.pop(slot)) {
                    rx_ring_buf.push(slot);
                }
            }
        }
    }
}