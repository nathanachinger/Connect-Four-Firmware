# Connect Four Firmware

Firmware for a handheld electronic Connect Four game built on an STM32G071. Play against a bot,
or pass and play with a friend. Pieces are shown on red and yellow LED grids, menus on a small
LCD, and a rotary encoder handles all input.

## Features

- **One-player mode** against a bot: Easy, Medium or Hard. The bot shows a "thinking" sweep,
  then slides its piece over and drops it
- **Two-player mode** with pass and play; the other player starts the next game
- **LED board**: the aiming piece blinks along the top row, pieces fall into place, and the
  winning line blinks
- **LCD menus** for mode, difficulty and settings, and a turn display during a game
- **Automatic brightness** for both the backlight and the LEDs, following room light from a
  photoresistor; it ignores your hand passing over the sensor
- **Sound effects** for menus, turning, dropping, landing and game results
- **Settings**: brightness (auto or 1–10), sound effects on/off and debug mode, saved across
  power cycles
- **Battery estimate** on the startup screen, with a low-battery warning
- **Auto off** after 5 minutes without input, into the MCU's lowest-power mode until the next
  power cycle
- **Debug mode**: bot insights on the LCD (who is ahead, the bot's search depth and positions
  searched, live while it thinks, the line of moves it expects, and a score for every column you
  could play), plus undo with a double-click
- **Pause menu** (hold during a game): resume, get a hint and quit. A hint shows, for the rest of
  your turn, how good the column under your cursor is (`COL 4: +22 BEST`, `WINS IN 2`,
  `LOSES NEXT`)

Planned: battery monitoring during play, with the low-battery LED and an automatic cutoff
(see [docs/ROADMAP.md](docs/ROADMAP.md)).

## Controls

| Action | Menu | Game |
|---|---|---|
| Turn | Move the highlight | Move your piece along the top row |
| Click | Choose | Drop the piece; after a game, play again |
| Hold (1.5 s) | Back | Pause menu: resume, get hint, quit |
| Double-click | | Debug mode only: undo (in 1-player, back to your last move), also after a game |

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

- Settings are saved in the last 2 KB page of flash (`0x0800F800`). Normal programming only
  erases the pages the firmware uses, so settings survive reflashing; a full-chip erase resets
  them to the defaults. The build fails if the firmware ever grows into that page.
- After auto off the chip is in Shutdown mode and the probe can't connect. Power cycle the
  board first, or use the programmer's connect-under-reset mode.

## Changing the hardware configuration

Pins and peripherals are set up in STM32CubeMX using `ConnectFour.ioc`. After changing them,
regenerate the code. Anything you've written inside `USER CODE BEGIN` / `USER CODE END` blocks
is kept when CubeMX regenerates.

## Status

Development is tracked as milestones in [docs/ROADMAP.md](docs/ROADMAP.md).

- [x] **M0**: build system, firmware entry point
- [x] **M1**: game rules and win detection
- [x] **M2**: bot (negamax with alpha-beta; easy, medium and hard)
- [x] **M3–M6**: hardware bring-up, LED matrix, input, display and menus
- [x] **M7**: first playable version (two-player)
- [x] **M8**: one-player mode against the bot
- [x] **M9**: sound effects
- [x] **M10**: automatic brightness
- [ ] **M11**: battery handling (startup estimate and auto off done; in-game monitoring, low-battery
  LED and cutoff to do)
- [ ] **M12**: polish (settings persistence done; watchdog to do)
