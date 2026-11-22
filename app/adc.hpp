#pragma once

#include <cstdint>

namespace adc {

// Shared ADC1 access. Each read switches the single regular channel, so the light sensor,
// battery and VREFINT can take turns. Safe to call init() more than once.
bool init();

// Average of `samples` 12-bit conversions on `channel` (an ADC_CHANNEL_x value).
// Returns 0 if the ADC fails.
uint16_t read(uint32_t channel, uint8_t samples = 16);

// Measures VDDA against the factory-calibrated internal reference and caches it.
uint32_t measure_vdda_mv();
// Last measured VDDA (3300 until the first measurement).
uint32_t vdda_mv();

// Converts a raw reading to millivolts at the pin, using the cached VDDA.
uint32_t to_mv(uint16_t raw);

}  // namespace adc
