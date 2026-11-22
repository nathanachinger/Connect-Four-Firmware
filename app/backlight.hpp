#pragma once

#include <cstdint>

namespace backlight {

// LCD backlight: SI2302 low-side MOSFET, gate on PA8 driven by TIM1_CH1 PWM at 20 kHz
// (above hearing range). Set up here because CubeMX doesn't configure TIM1 yet.
bool init();

// Brightness 0..100, roughly perceptual: duty = percent^2 / 100, so low settings get
// finer steps where the eye is most sensitive.
void set_percent(uint8_t percent);
uint8_t percent();

}  // namespace backlight
