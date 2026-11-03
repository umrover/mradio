#include "main.h"
#include "EthDriver.hpp"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"

namespace mrover {
    extern "C" {
        void lwipInit() {
            EthDriver::getInstance().init();
        }
    }
}