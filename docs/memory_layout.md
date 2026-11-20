# Flash and RAM layout

From `arm-none-eabi-size -A` built with Release

| Region                 | Address      | Size    | Used by                                      |
| ---------------------- | ------------ | ------- | -------------------------------------------- |
| Bank 1, sectors 0 to 7 | `0x08000000` | 1024 KB | CM7 image, `stm32h745xx_flash_CM7.ld`        |
| Bank 2, sector 0       | `0x08100000` | 128 KB  | CM4 image, `CM4/flash_limit.ld`              |
| Bank 2, sectors 1 to 6 | `0x08120000` | 768 KB  | CM7 Wi-Fi driver and firmware, `CM7/wifi.ld` |
| Bank 2, sector 7       | `0x081E0000` | 128 KB  | Saved config, `storage.cpp`                  |

| Region          | Address      | Size   | Used by                    | Cache                    |
| --------------- | ------------ | ------ | -------------------------- | ------------------------ |
| ITCM            | `0x00000000` | 64 KB  | Unused                     | Not cached               |
| DTCM            | `0x20000000` | 128 KB | Unused                     | Not cached               |
| AXI SRAM        | `0x24000000` | 512 KB | CM7 data, heaps and stacks | D-cache                  |
| SRAM1 and SRAM2 | `0x10000000` | 256 KB | CM4 data, heaps and stacks | CM4 has no cache         |
| SRAM3           | `0x30040000` | 32 KB  | Ethernet DMA               | Uncached by MPU region 1 |
| SRAM4           | `0x38000000` | 64 KB  | Unused                     |                          |
| Backup SRAM     | `0x38800000` | 4 KB   | Unused                     |                          |
