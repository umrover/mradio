#include "main.h"

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