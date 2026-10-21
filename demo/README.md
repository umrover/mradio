# demo

HaLow link between two NUCLEO-H755ZI-Q boards, each with a Seeed Wi-Fi HaLow board for XIAO (FGH100M-H).
Built from the MM-IoT-SDK 2.13.1 `ap_mode` and `ping` examples and its Nucleo-H753 platform. Only the CM7 core is used.

- Board A runs `ap_mode`. It is the HaLow access point at `192.168.1.1` and blinks its green LED (LD1) for every message it receives on UDP port 5000.
- Board B runs `station`. It joins board A as a station at `192.168.1.2` and sends one UDP message per press of its blue user button (B1), blinking its own green LED when the send succeeds.
- SSID `mradio`, SAE passphrase `mradiodemo`, country `US`. All of it is compiled in, so there is no config store to program.

## Wiring

Same on both boards. The XIAO pin numbers are the castellated header (CN2) on the HaLow board.

| HaLow board (XIAO pin) | Signal  | Nucleo pin | Nucleo header |
|------------------------|---------|------------|---------------|
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

## Build and flash

Plug the board into USB. The Nucleo's USB port is its ST-LINK, so the same cable does flashing and serial.

```sh
git submodule update --init demo/mm-iot-sdk
./demo/scripts/flash.sh a    # board A, access point
./demo/scripts/flash.sh b    # board B, station with the button
```

With both boards plugged in at once, add the ST-LINK serial of the one to flash, from `STM32_Programmer_CLI --list`:

```sh
./demo/scripts/flash.sh a <board A serial>
```

- The script builds with `make APP=ap_mode` or `make APP=station` and then runs `make flash`. Those work directly from `demo/` too.
- Flashing erases the whole chip, including the CM4 bank.
- The build uses the STM32CubeCLT gcc unless Morse's toolchain is in `/opt/morse`.

## Check it works

Open both ST-LINK serial ports at 115200 baud, for example `picocom -b 115200 /dev/ttyACM0`. Reset board A first, then board B. Board B prints `Ready` once it has joined. Each press of its blue button then prints `Sent press N` on board B, `Received 'press N'` on board A, and blinks board A's green LED.

The red LED on either board means startup failed; the console says why.

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
