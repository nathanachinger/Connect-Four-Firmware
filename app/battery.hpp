#pragma once

#include <cstdint>

namespace battery {

// +VBAT_SW through a 120k/100k divider to PA0 (ADC1_IN0): Vbat = Vpin * 2.2.
// Four AA cells: about 6.5 V fresh, 4.0 V at cutoff.
bool init();

// Battery voltage in millivolts, averaged over 16 conversions. Returns 0 if the ADC fails.
uint32_t read_mv();

}  // namespace battery
