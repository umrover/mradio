#include "main.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"

namespace mrover {
    void event_loop() {
        while (true) {
            
        }
    }

    void init() {
        event_loop();
    }
}

extern "C" {
    void PostInit() {
        mrover::init();
    }
}