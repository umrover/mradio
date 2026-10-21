# demo

## Wiring

Same on both boards. The XIAO pin numbers are the castellated header (CN2) on the HaLow board.

| HaLow board (XIAO pin) | Signal  | Nucleo pin | Nucleo header |
| ---------------------- | ------- | ---------- | ------------- |
| 1 (D0)                 | RESET_N | PA3        | A0            |
| 3 (D2)                 | SPI_INT | PC2        | A4            |
| 4 (D3)                 | SPI_CS  | PD14       | D10           |
| 8 (D8)                 | SCK     | PA5        | D13           |
| 9 (D9)                 | MISO    | PA6        | D12           |
| 10 (D10)               | MOSI    | PB5        | D11           |
| 12                     | 3V3     | 3V3        | CN8           |
| 13                     | GND     | GND        | CN8           |
| 14                     | 5V      | 5V         | CN8           |

- Leave D1 (WAKE) and D4 (BUSY) unconnected. Seeed does not populate those resistors, so the firmware treats the chip as always awake.
- 5V is required. It powers the module's RF front end.
- Connect the antenna before powering up. Do not transmit into an open U.FL.

## Changes from the Morse H753 platform

Unmodified SDK files are compiled in place from `mm-iot-sdk`. Only the files changed for this board live in `platform/`.

- `main.c`
  - Direct SMPS supply instead of LDO. The H755 Nucleo is wired for SMPS, and the wrong setting locks the board.
  - VOS1 at 400 MHz, since VOS0 needs the LDO.
  - SPI at 10 MHz instead of 40 MHz, because the boards are on jumper wires.
  - PC2 and PC3 analog switches are closed so SPI_INT reaches the digital input.
  - VTOR is reset to the base of flash, because the SDK's `SystemInit` expects a bootloader in front of the image.
- `flash.ld` puts the image at `0x08000000` across the full 1 MB CM7 bank. The config store sits in the last CM4 sector.
- `mmhal_os.c` reports the 400 MHz clock.
- `mmhal_app.c` reads the blue button.
- `mmhal_wlan.c` reports BUSY as always asserted, and `station.cpp` turns power save off.
- `mmhal_flash.c` fixes the SDK's last partial flash write, which passed the data instead of a pointer to it and hard-faulted on first boot.
