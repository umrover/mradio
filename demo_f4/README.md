# demo_f4

Port of `demo/` to a NUCLEO-F439ZI for demos. `src`, the Makefile, and `scripts/flash.sh` are shared with `demo/`; only `platform/` and `platform.mk` are F4-specific. The Morse SDK comes from `../demo/mm-iot-sdk`.

## Wiring

Two pins sit on different headers than on the H755 board, so do not copy the `demo/` wiring.

| HaLow board (XIAO pin) | Signal  | F439 pin | Nucleo header |
|------------------------|---------|----------|---------------|
| 1 (D0)                 | RESET_N | PA3      | CN9 A0        |
| 3 (D2)                 | SPI_INT | PC3      | CN9 A2        |
| 4 (D3)                 | SPI_CS  | PD14     | CN7 D10       |
| 8 (D8)                 | SCK     | PA5      | CN7 D13       |
| 9 (D9)                 | MISO    | PA6      | CN7 D12       |
| 10 (D10)               | MOSI    | PB5      | CN7 D22       |
| 12                     | 3V3     | 3V3      | CN8           |
| 13                     | GND     | GND      | CN8           |
| 14                     | 5V      | 5V       | CN8           |

- MOSI is on D22, not D11. D11 is PA7 on this board, and PA7 is shared with the Ethernet PHY through JP6.
- Leave D1 (WAKE) and D4 (BUSY) unconnected, same as `demo/`.
- 5V is required. It powers the module's RF front end.
- Connect the antenna before powering up.

## Build and flash

```
git submodule update --init demo/mm-iot-sdk demo_f4/drivers
./demo_f4/scripts/flash.sh a      # access point
./demo_f4/scripts/flash.sh b      # station
```

Logs are on the ST-LINK serial port at 115200 baud.

## Differences from the H755 port

- Runs at 168 MHz from HSI, so the clock does not depend on the ST-LINK MCO solder bridges.
- Uses the LL drivers only.
    - Pinned at stm32f4xx-hal-driver v1.8.5 and cmsis-device-f4 v2.6.9.
- SPI1 runs at 10.5 MHz with DMA2 stream 0 for RX and stream 3 for TX.
- There is no config store. The flash shim is read-only, and settings are compiled in.
- There is no RTC. `mmhal_get_time` counts from boot.
