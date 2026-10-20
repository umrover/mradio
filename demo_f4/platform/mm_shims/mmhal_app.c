/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_app.h"
#include "mm_hal_common.h"
#include "mmosal.h"
#include "mmutils.h"

#include "main.h"

static mmhal_button_state_cb_t mmhal_button_state_cb = NULL;

void mmhal_set_led(uint8_t led, uint8_t level)
{
    switch (led)
    {
        case LED_RED:
            if (level)
            {
                LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
            }
            else
            {
                LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
            }
            break;

        case LED_GREEN:
            if (level)
            {
                LL_GPIO_SetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
            }
            else
            {
                LL_GPIO_ResetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
            }
            break;

        case LED_BLUE:
            /* The h753 doesn't have a blue led. For hal completeness, yellow is treated as blue. */
            if (level)
            {
                LL_GPIO_SetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
            }
            else
            {
                LL_GPIO_ResetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
            }
            break;

        default:
            break;
    }
}

void mmhal_set_error_led(bool state)
{
    if (state)
    {
        LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    }
    else
    {
        LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    }
}

enum mmhal_button_state mmhal_get_button(enum mmhal_button_id button_id)
{
    if (button_id != BUTTON_ID_USER0)
    {
        return BUTTON_RELEASED;
    }
    /* Nucleo B1 (blue button) on PC13 is active high */
    return LL_GPIO_IsInputPinSet(B1_GPIO_Port, B1_Pin) ? BUTTON_PRESSED : BUTTON_RELEASED;
}

bool mmhal_set_button_callback(enum mmhal_button_id button_id,
                               mmhal_button_state_cb_t button_state_cb)
{
    if (button_id != BUTTON_ID_USER0)
    {
        return false;
    }

    mmhal_button_state_cb = button_state_cb;
    return true;
}

mmhal_button_state_cb_t mmhal_get_button_callback(enum mmhal_button_id button_id)
{
    if (button_id != BUTTON_ID_USER0)
    {
        return NULL;
    }

    return mmhal_button_state_cb;
}

/* No RTC on this port; wall time is an offset from the scheduler clock and resets on boot */
static time_t epoch_at_boot = 0;

time_t mmhal_get_time()
{
    return epoch_at_boot + (time_t)(mmosal_get_time_ms() / 1000);
}

void mmhal_set_time(time_t epoch)
{
    epoch_at_boot = epoch - (time_t)(mmosal_get_time_ms() / 1000);
}

void mmhal_set_debug_pins(uint32_t mask, uint32_t values)
{
    MM_UNUSED(mask);
    MM_UNUSED(values);
}

bool mmhal_get_hardware_version(char *version_buffer, size_t version_buffer_length)
{
    return !mmosal_safer_strcpy(version_buffer, MMHAL_HARDWARE_VERSION, version_buffer_length);
}
