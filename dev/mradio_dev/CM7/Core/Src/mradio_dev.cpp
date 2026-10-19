#include "main.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"

namespace mrover {
    void event_loop() {
        while (true) {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
            HAL_Delay(500);
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