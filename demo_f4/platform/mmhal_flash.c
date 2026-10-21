/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "mmhal_flash.h"
#include "mmosal.h"
#include "mmutils.h"

/* No partition makes mmconfig return not-supported, so the compiled-in settings are used */
const struct mmhal_flash_partition_config *mmhal_get_mmconfig_partition(void)
{
    return NULL;
}

uint32_t mmhal_flash_getblocksize(uint32_t block_address)
{
    MM_UNUSED(block_address);
    return 0;
}

/* ponytail: read-only, settings are compiled in; port the H7 erase/program code if mmconfig is ever enabled */
int mmhal_flash_erase(uint32_t block_address)
{
    MM_UNUSED(block_address);
    return -1;
}

int mmhal_flash_read(uint32_t read_address, uint8_t *buf, size_t size)
{
    memcpy(buf, (void *)read_address, size);
    return 0;
}

int mmhal_flash_write(uint32_t write_address, const uint8_t *data, size_t size)
{
    MM_UNUSED(write_address);
    MM_UNUSED(data);
    MM_UNUSED(size);
    return -1;
}
