# Connect Four Firmware

Firmware for a handheld electronic Connect Four game built on an STM32G071. Play against a bot,
or pass and play with a friend. Pieces are shown on red and yellow LED grids, menus on a small
LCD, and a rotary encoder handles all input.

## Features

- **One-player mode** against a bot with selectable difficulty
- **Two-player mode** with pass and play
- **Rotary encoder controls**: turn to move your piece along the top row, click to drop it,
  hold to return to the menu
- **LCD menu** for choosing a mode, and a turn display during a game
- **Automatic backlight** that follows room brightness, read from a photoresistor
- **Sound effects** for menu navigation, turning, clicking, wins and losses
- **Battery monitoring** with a low-battery LED, and an automatic cutoff that puts the device
  into a low-power state before the batteries run too low

## Hardware

The full schematic is in [docs/connect_four_schematic.pdf](docs/connect_four_schematic.pdf).

| Function | Part |
|---|---|
| Microcontroller | STM32G071C8T6 (Cortex-M0+, 64 KB flash, 36 KB RAM) |
| Game board | Two 7×7 LED grids (red and yellow), driven by an IS31FL3731 over I2C |
| Display | NHD-C12832A1Z 128×32 LCD (ST7565R) over SPI, PWM-dimmed backlight |
| Input | PEC11R rotary encoder with push button |
| Ambient light | PDV-P8103 photoresistor |
| Audio | DAC output into a PAM8302 class-D amplifier |
| Power | Four AA batteries (6.5–4 V combined), TPS62160 buck converter to 3.3 V |

The 7×7 grid is a standard 6-row by 7-column Connect Four board with one extra row on top,
where the current piece hovers before it drops.

## Project structure

```
├─ Core/, Drivers/   STM32CubeMX generated code (main.c calls app_main())
├─ app/              Application code (C++, uses the STM32 HAL directly)
├─ cmake/            Toolchain files and CubeMX CMake glue
└─ docs/             Schematic and roadmap
```

## Building

### Requirements

- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
  (`arm-none-eabi-gcc`) on your `PATH`
- CMake 3.22 or newer
- Ninja

```bash
cmake --preset Debug
cmake --build --preset Debug
```

This produces `ConnectFour.elf`, `ConnectFour.hex` and `ConnectFour.bin` in `build/Debug/`.
Use the `Release` preset for a size-optimized build.

## Flashing

Connect a debug probe to the SWD header and flash `build/Debug/ConnectFour.elf` with any
SWD-capable tool, such as STM32CubeProgrammer, STM32CubeIDE, or SEGGER J-Link and Ozone.

## Changing the hardware configuration

Pins and peripherals are set up in STM32CubeMX using `ConnectFour.ioc`. After changing them,
regenerate the code. Anything you've written inside `USER CODE BEGIN` / `USER CODE END` blocks
is kept when CubeMX regenerates.

## Status

Development is tracked as milestones in [docs/ROADMAP.md](docs/ROADMAP.md).

- [x] **M0**: build system, firmware entry point
- [ ] **M1**: game rules and win detection
- [ ] **M2**: bot
- [ ] **M3–M6**: hardware bring-up (LED matrix, encoder, display)
- [ ] **M7**: first playable version (two-player)
- [ ] **M8–M12**: one-player mode, audio, backlight, battery handling, polish
