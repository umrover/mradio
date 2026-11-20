#pragma once

#include "stm32h7xx_hal.h"
#include "PacketInfo.hpp"
#include "cmsis_os2.h"
#include <cstdint>

extern ETH_HandleTypeDef heth;

namespace mrover {
    // class for interfacing with ethernet peripherals
    class EthDriver {
    private:
        EthDriver() = default;

        // rtos task and semaphore
        osThreadId_t rx_task = nullptr;
        osSemaphoreId_t rx_semaphore = nullptr;

        // entry function for rx task
        static void rxTaskEntry(void* argument);

    public:
        // singleton access
        static EthDriver& getInstance() {
            static EthDriver instance;
            return instance;
        }

        EthDriver(const EthDriver&) = delete;
        EthDriver& operator=(const EthDriver&) = delete;
        EthDriver(EthDriver&&) = delete;
        EthDriver& operator=(EthDriver&&) = delete;

        // initialize EthDriver
        void init();

        // transmit an ethernet frame
        bool sendFrame(const uint8_t* frame, uint16_t len);

        // signal incoming packet
        void notifyRxFromISR();
    };
}