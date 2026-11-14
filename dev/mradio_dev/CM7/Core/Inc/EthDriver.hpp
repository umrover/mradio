#pragma once

#include "stm32h7xx_hal.h"
#include "PacketInfo.hpp"
#include <cstdint>

extern ETH_HandleTypeDef heth;

namespace mrover {
    // class for interfacing with ethernet peripherals
    class EthDriver {
    private:
        EthDriver() = default;

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
    };
}