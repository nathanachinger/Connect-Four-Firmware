# Connect Four — Architecture & Roadmap

Hardware reference: [connect_four_schematic.pdf](connect_four_schematic.pdf)

## Hardware summary

| Function | Part | MCU resource |
|---|---|---|
| MCU | STM32G071C8T6 (Cortex-M0+, 64 KB flash, 36 KB RAM, no FPU/divider) | — |
| LED board (2× 7×7, red + yellow) | IS31FL3731 charlieplex driver, AD→GND (addr 0x74), SDB pulled high | I2C1 (PA9/PA10) |
| Display | NHD-C12832A1Z (ST7565R, 128×32, write-only) | SPI2 (PB10/PB11), CS PB8, A0 PB9, nRST PB7 |
| Display backlight | SI2302 low-side MOSFET | TIM1_CH1 PWM (PA8) |
| Rotary encoder | PEC11R-4215K-S0012 (2 counts per detent, measured) | TIM3 encoder (PA6/PA7), push = ENCODER_SEL (PA5, ext. pull-up, active low) |
| Ambient light | PDV-P8103 photoresistor + 10k to GND (brighter → higher V) | ADC1_IN8 (PB0) |
| Battery sense | 120k/100k divider on +VBAT_SW (Vbat = Vadc × 2.2) | ADC1_IN0 (PA0) |
| Low battery LED | Red LED + 270 Ω | PA1 |
| Audio | PAM8302 class-D amp, SD = AMP_EN (100k pull-down) | DAC1_OUT1 (PA4), AMP_EN (PA2) |
| Power | 6.5–4 V battery → TPS62160 buck (EN tied high) → 3.3 V | — |

Notes:
- The buck is always enabled, so firmware cannot remove its own power. "Off" = MCU Shutdown mode;
  the floor current is set by the battery divider (~25 µA) and buck quiescent current, not the MCU.
- PA5 is not a WKUP pin, so leaving Shutdown requires a power cycle (S2) or reset.
- The 7×7 grid = 1 cursor row on top + standard 6×7 Connect Four board.

## Layout

```
ConnectFour/
├─ Core/ Drivers/ cmake/stm32cubemx   # CubeMX-owned; only USER CODE hooks in main.c call app_main()
└─ app/        # All application code: game logic, bot, chip drivers, input, UI, services
```

Runtime model: bare-metal superloop, 1 ms SysTick time base, no heap, no exceptions/RTTI.
Audio runs on DMA and the encoder count lives in hardware, so a long bot search never drops input or sound
(the search's progress callback also keeps the button debounce, sound sequencing and LED animation going).

## Builds

```bash
cmake --preset Debug && cmake --build --preset Debug
```

## Milestones

| # | Goal | Done when | Status |
|---|------|-----------|--------|
| 0 | Build foundation: CMake, `app_main()` hook, CubeMX fixes | Firmware builds and blinks LOW_BAT_LED | ✅ Done |
| 1 | Game rules: board, drop, legal moves, win/draw detection, winning cells | All 4 win directions, full columns, draws work | ✅ Done (host-checked against a brute-force solver) |
| 2 | Bot | Difficulty levels behave as expected; nodes/sec measured | ✅ Done; nodes/sec measured on the host, and shown on target in debug mode |
| 3 | Peripheral bring-up: clock, GPIO, I2C, SPI, ADC, encoder timer | Each verified on hardware | ✅ Done (hardware test on the `hw-test` branch) |
| 4 | IS31FL3731 driver + LED map + renderer | Test pattern correct; known positions render right | ✅ Done (grid is transposed vs. the schematic) |
| 5 | Input: detent accumulator + button gestures + event queue | Timing edges handled; cursor moves on hardware | ✅ Done (polled events, no queue needed) |
| 6 | ST7565 driver + canvas + font + menu screen | 1P/2P menu renders and scrolls | ✅ Done |
| 7 | App state machine, 2-player pass-and-play | **First playable**: full game, win highlight, hold → menu | ✅ Done |
| 8 | 1-player on target | Bot moves within time budget with thinking indicator; difficulty select | ✅ Done |
| 9 | Audio: TIM6 + DMA + synth + sound events | Menu/rotate/click/win/lose cues, no pops | ✅ Done |
| 10 | Auto backlight | Smooth tracking of room light | ✅ Done (also drives LED brightness) |
| 11 | Battery monitor + low LED + cutoff + Shutdown | Bench-supply sweep; sleep current measured | 🟡 Partial: startup estimate and Shutdown (auto off) done; in-game monitoring, low-battery LED and cutoff not yet; sleep current not measured |
| 12 | Polish: animations, IWDG watchdog, flash budget, optional settings persistence | Release build fits comfortably | 🟡 Partial: animations, settings persistence, sound/debug settings and a flash-budget check done; IWDG not yet |

### Remaining work

- **Battery (M11)**: periodic VREFINT-compensated readings during play, low-battery LED at ~4.4 V,
  cutoff to Shutdown at ~4.0 V; bench-supply sweep; measure the Shutdown floor current.
- **Watchdog (M12)**: IWDG, refreshed from the main loop and the bot's progress callback.
- **Flash ECC at boot**: a settings write cut off by power loss can leave a word that raises an
  NMI when read; handle it in `NMI_Handler` (erase the settings page, reset).
- **Flash budget**: Debug builds use ~83% of flash (Release ~47%), leaving ~9 KB below the
  settings page. The post-build check fails the build if the image reaches it.
- **Bot strength (optional)**: a 4–8 KB transposition table would let Hard search deeper.

## Design notes (as built)

- **Input** (`input.cpp`): `Knob` turns TIM3 counts into detents, carrying the remainder so a
  read mid-click never loses a step; this encoder gives **2 counts per detent** (measured).
  `Button` is pure logic fed `(pressed, now_ms)`: 20 ms debounce, click on release, hold at 1.5 s
  fires *while held* (hold = back / leave a game). Polled every 2 ms from the superloop.
- **Game rules** (`board.cpp`): one `uint64_t` per player, 7 bits per column (sentinel bit stops
  wrap), wins found by shift-and-AND in 4 directions, returning every winning cell.
- **Bot** (`bot.cpp`): negamax + alpha-beta on (current, occupied) bitboards, center-first
  ordering, immediate-win check, forced-block and "never play under their winning cell"
  filtering; evaluation = threats (×24) + per-cell line counts. Easy: win, else block, else
  random. Medium: 4 plies. Hard: iterative deepening for up to 2 s (stops once the result is
  forced). Ties broken at random; RNG seeded from ADC noise and click timing. Compiled at `-O2`
  even in Debug. No transposition table. Search can nest 42 deep (112 B/level), so the reserved
  stack is 6 KB.
- **LED map** (`led_matrix.cpp`): the grid is **transposed** relative to the schematic's Cx-y
  names; `set(row, col)` is physical (row 0 top, col 0 left). Win highlight blinks the winning
  pieces in their own colour: red+yellow "orange" reads as red on this hardware.
- **Game screen** (`match.cpp`): aiming piece blinks (solid while moving), pieces fall row by row,
  bot shows a sweeping "thinking" piece, then slides to its column. Against the bot the player
  is red; who starts alternates.
- **Brightness** (`settings.cpp`): one level (AUTO or manual 1–10) drives both the backlight and
  the LED board through separate 10-point curves. Both keep their dark and very-bright ends;
  the LED curve is held very low through normal room light. AUTO: the room level is the
  brightest light reading of the last 2.5 s (so hand shadows are ignored), then averaged
  (brightening faster than darkening), with a deadband and a smooth fade.
- **Audio** (`audio.cpp`, `sound.cpp`): TIM6 TRGO triggers DAC conversions fed by circular DMA;
  half/complete interrupts synthesize the next half-buffer at 16 kHz. Effects are note
  sequences with priorities (ticks < clicks < results). Sound is on/off at full DAC swing (the
  amp's designed ~5.4 Vpp). The amp is on only while sounds play, off 0.3 s after the last one.
- **Settings** (`settings.cpp`, `flash_store.cpp`): brightness, sound and debug, saved as
  append-only 8-byte records (magic, version, payload, CRC-8) in the last 2 KB flash page;
  the page is erased only when full. Saved on leaving the settings screen and before auto off,
  skipped below 4.0 V.
- **Debug mode**: in 1-player games shows the bot's last search (depth, nodes, time) and who it
  thinks is ahead; in 2-player games, a quick 5-ply search shows who is ahead.
- **Battery** (`battery.cpp`): pack voltage from the divider with VDDA measured against VREFINT;
  shown at startup as a percent along an alkaline discharge curve (4.0 V = 0%).
- **Auto off / Shutdown** (`power.cpp`): after 5 minutes without input: speaker off → IS31
  software shutdown → display sleep → backlight off → STM32 Shutdown, no wake source. PWR
  pull-ups/downs hold only the display lines that lack external resistors (PB8 up, PB9–PB11
  down); they are cleared again at boot.

## M0 CubeMX checklist (do in CubeMX, then regenerate)

User code in `USER CODE` blocks survives regeneration.

- [x] **PA5 ENCODER_SEL** → `GPIO_Input`, no pull (R1 is the pull-up).
- [x] **Clock** → SYSCLK from PLLR = 64 MHz.
- [x] **TIM1 CH1 (PA8)** → PWM Generation CH1 for backlight. At 64 MHz: PSC = 0, ARR = 3199 → 20 kHz.
- [x] **TIM6** → used as DAC trigger: Trigger Event Selection = Update Event (period set in code later).
- [x] **DAC1 OUT1** → Trigger = Timer 6 Trigger Out event; add DMA request DAC1_CH1, circular, half-word.
- [x] **ADC1** → polled: each read switches the single regular channel (IN0, IN8 or VREFINT); 160.5-cycle sample time for the high-impedance sources.
- [x] Confirm **I2C1** timing is 400 kHz Fast Mode for the new clock.
