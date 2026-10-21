/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_core.h"
#include "mm_hal_common.h"
#include "mmosal.h"
#include "mmutils.h"

#include "main.h"

#define RNG_MAX_GENERATE_ATTEMPTS (100)
#define RNG_READY_SPIN_LIMIT      (10000)

static struct mmosal_mutex *rng_mutex = NULL;

void mmhal_random_init(void)
{
    MMOSAL_DEV_ASSERT(rng_mutex == NULL);
    if (rng_mutex != NULL)
    {
        return;
    }

    rng_mutex = mmosal_mutex_create("rng");
    MMOSAL_ASSERT(rng_mutex != NULL);
}

static bool rng_read(uint32_t *value)
{
    if (LL_RNG_IsActiveFlag_SECS(RNG) || LL_RNG_IsActiveFlag_CECS(RNG))
    {
        LL_RNG_ClearFlag_SEIS(RNG);
        LL_RNG_ClearFlag_CEIS(RNG);
        LL_RNG_Disable(RNG);
        LL_RNG_Enable(RNG);
        return false;
    }

    for (uint32_t spins = 0; spins < RNG_READY_SPIN_LIMIT; spins++)
    {
        if (LL_RNG_IsActiveFlag_DRDY(RNG))
        {
            *value = LL_RNG_ReadRandData32(RNG);
            return true;
        }
    }
    return false;
}

uint32_t mmhal_random_u32(uint32_t min, uint32_t max)
{
    MMOSAL_DEV_ASSERT(rng_mutex);

    uint32_t rndm = 0;
    bool ok = false;
    for (uint32_t ii = 0; ii < RNG_MAX_GENERATE_ATTEMPTS && !ok; ii++)
    {
        mmosal_mutex_get(rng_mutex, UINT32_MAX);
        ok = rng_read(&rndm);
        mmosal_mutex_release(rng_mutex);
    }
    MMOSAL_ASSERT(ok);

    if (min == 0 && max == UINT32_MAX)
    {
        return rndm;
    }
    return rndm % (max - min + 1) + min;
}

void mmhal_set_deep_sleep_veto(uint8_t veto_id)
{
    MM_UNUSED(veto_id);
}

void mmhal_clear_deep_sleep_veto(uint8_t veto_id)
{
    MM_UNUSED(veto_id);
}
