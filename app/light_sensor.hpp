#pragma once

#include <cstdint>

namespace light {

// PDV-P8103 photoresistor from 3V3 to PB0 (ADC1_IN8) with 10k to GND: brighter -> higher reading.
bool init();

// Average of `samples` 12-bit conversions (0..4095). Returns 0 if the ADC fails.
uint16_t read_raw(uint8_t samples = 16);

uint32_t raw_to_mv(uint16_t raw);

}  // namespace light
