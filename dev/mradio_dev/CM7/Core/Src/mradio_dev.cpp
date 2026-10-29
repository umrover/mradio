#include "main.h"
#include "tcpip.h"

namespace mrover {
void init() {}
} // namespace mrover

extern "C" {
void PostInit() { 
    mrover::init();

    LOCK_TCPIP_CORE();
    httpd_init();
    UNLOCK_TCPIP_CORE();
}
}
