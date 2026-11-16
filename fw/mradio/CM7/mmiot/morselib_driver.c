/* ponytail: driver_data is static in driver.c, so it is built here to expose it without patching the SDK submodule */
#include "driver/driver.c"

struct driver_data *mrover_driver_data(void)
{
    return &driver_data;
}
