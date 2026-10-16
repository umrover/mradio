#include "lwip.h"
#include "main.h"

namespace mrover {
void event_loop() {
    while (true) {
        MX_LWIP_Process();
    }
}

void init() { event_loop(); }
} // namespace mrover

extern "C" {
void PostInit() { mrover::init(); }
}
