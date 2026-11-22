# Connect Four — Architecture & Roadmap

Hardware reference: [connect_four_schematic.pdf](connect_four_schematic.pdf)

## Hardware summary

| Function | Part | MCU resource |
|---|---|---|
| MCU | STM32G071C8T6 (Cortex-M0+, 64 KB flash, 36 KB RAM, no FPU/divider) | — |
| LED board (2× 7×7, red + yellow) | IS31FL3731 charlieplex driver, AD→GND (addr 0x74), SDB pulled high | I2C1 (PA9/PA10) |
| Display | NHD-C12832A1Z (ST7565R, 128×32, write-only) | SPI2 (PB10/PB11), CS PB8, A0 PB9, nRST PB7 |
| Display backlight | SI2302 low-side MOSFET | TIM1_CH1 PWM (PA8) |
| Rotary encoder | PEC11R-4215K-S0012 | TIM3 encoder (PA6/PA7), push = ENCODER_SEL (PA5, ext. pull-up, active low) |
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
Audio runs on DMA and the encoder count lives in hardware, so a long bot search never drops input or sound.

## Builds

```bash
cmake --preset Debug && cmake --build --preset Debug
```

## Milestones

| # | Goal | Done when |
|---|------|-----------|
| 0 | Build foundation: CMake, `app_main()` hook, CubeMX fixes | Firmware builds and blinks LOW_BAT_LED |
| 1 | Game rules: board, drop, legal moves, win/draw detection, winning cells | All 4 win directions, full columns, draws work |
| 2 | Bot | Difficulty levels behave as expected; nodes/sec measured |
| 3 | Peripheral bring-up: clock, GPIO, I2C, SPI, ADC, encoder timer | Each verified on hardware |
| 4 | IS31FL3731 driver + LED map + renderer | Test pattern correct; known positions render right |
| 5 | Input: detent accumulator + button gestures + event queue | Timing edges handled; cursor moves on hardware |
| 6 | ST7565 driver + canvas + font + menu screen | 1P/2P menu renders and scrolls |
| 7 | App state machine, 2-player pass-and-play | **First playable**: full game, win highlight, hold → menu |
| 8 | 1-player on target | Bot moves within time budget with thinking indicator; difficulty select |
| 9 | Audio: TIM6 + DMA + synth + sound events | Menu/rotate/click/win/lose cues, no pops |
| 10 | Auto backlight | Smooth tracking of room light |
| 11 | Battery monitor + low LED + cutoff + Shutdown | Bench-supply sweep; sleep current measured |
| 12 | Polish: animations, IWDG watchdog, flash budget, optional settings persistence | Release build fits comfortably |

## Design notes

- **Button gestures**: pure class fed `(pressed, now_ms)`. 20 ms debounce; click = release < ~400 ms;
  hold ≈ 1.5 s fires *while held* (with audio cue) → return to menu.
- **Encoder**: detent accumulator converts TIM3 counts to detents, carrying remainder.
  Counts-per-detent is a constant — verify empirically.
- **Bot**: 49-bit bitboard in `uint64_t`, negamax + alpha-beta, center-first ordering,
  iterative deepening with deadline callback; optional 4–8 KB transposition table.
  Difficulty: easy (random but takes/blocks wins), medium (depth ~4), hard (time-budgeted).
  RNG seeded from ADC noise.
- **LED renderer**: `(row, col, color) → driver index` table, transcribed from the schematic;
  LED-walk test mode to verify. Use IS31 frames for tear-free double buffering. Red+yellow = orange highlight.
- **Backlight**: EMA-filtered ambient → perceptual LUT → slew-limited, hysteretic duty. Reuse ambient
  level to scale LED matrix PWM.
- **Battery**: VREFINT-compensated, averaged over seconds (speaker sag), warn ≈ 4.4 V, cutoff ≈ 4.0 V,
  N consecutive samples + hysteresis. Check before enabling peripherals at boot.
- **Shutdown sequence**: stop DAC, AMP_EN low → IS31 software shutdown → display off + sleep →
  backlight pin low → flash low-bat LED, then off → MCU Shutdown mode, no wake source.
- **Audio**: sound IDs → note sequences synthesized into DMA half-buffers (~16 kHz); priority so a
  click doesn't cut off a jingle; DAC to mid-scale before AMP_EN; amp enabled only while playing.

## M0 CubeMX checklist (do in CubeMX, then regenerate)

User code in `USER CODE` blocks survives regeneration.

- [ ] **PA5 ENCODER_SEL** → `GPIO_Input`, no pull (R1 is the pull-up). Currently configured as output.
- [ ] **Clock** → SYSCLK from PLLR = 64 MHz (PLL is already set up for 64 MHz but HSI16 is selected).
- [ ] **TIM1 CH1 (PA8)** → PWM Generation CH1 for backlight. At 64 MHz: PSC = 0, ARR = 3199 → 20 kHz.
- [ ] **TIM6** → used as DAC trigger: Trigger Event Selection = Update Event (period set in code later).
- [ ] **DAC1 OUT1** → Trigger = Timer 6 Trigger Out event; add DMA request DAC1_CH1, circular, half-word.
- [ ] **ADC1** → scan IN0, IN8, VREFINT; consider DMA or polled sequence; long sample time (high-Z sources).
- [ ] Confirm **I2C1** timing is 400 kHz Fast Mode for the new clock.
