# airoc

The subset of Infineon's [AIROC-Wi-Fi-Bluetooth-STM32](https://github.com/Infineon/AIROC-Wi-Fi-Bluetooth-STM32) pack that drives the Ezurio Sona IF573 (CYW55572) over SDIO.

- Version
  - `release-v1.8.0`, the same pack wifi_demo generates from CubeMX
- Kept
  - `wifi-host-driver/COMPONENT_WIFI6` with only the CYW55572 firmware and the IF573 CLM and NVRAM
  - `whd-bsp-integration`, `wifi-connection-manager` without WPS, `lwip-network-interface-integration`, `connectivity-utilities` and `pal`
  - The pack's own lwIP and FreeRTOS are left out, so the morselib copies are used
- Local changes
  - `cy_network_mw_core.c` starts with `is_tcp_initialized` true, since `mmipal_init` already called `tcpip_init`
  - `stm32_cyhal_uart_ex.h` and `stm32_cyhal_lptimer_ex.h` are only included from `wifi_bt_if.c` when HAL UART and LPTIM are enabled, since this firmware drives its UART through LL
  - `stm32_cyhal_sdio_irq_handler` treats data and command timeouts as errors, since otherwise the flag stays set and the ISR re-enters forever
  - The raw lwIP calls in `cy_network_mw_core.c` and `cy_nw_helper.c` take `LOCK_TCPIP_CORE`, since the morselib lwIP port asserts the core lock
  - `CYBSP_WIFI_SDIO_1BIT` from `cybsp.h` picks a 1-bit bus in `cybsp_wifi.c` and `stm32_cyhal_sdio.c`, for a board whose D1 to D3 lines fail
