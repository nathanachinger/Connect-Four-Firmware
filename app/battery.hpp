#pragma once

#include <cstdint>

namespace battery {

// +VBAT_SW through a 120k/100k divider to PA0 (ADC1_IN0): Vbat = Vpin * 2.2.
// Four AA cells: about 6.5 V fresh, 4.0 V at cutoff.
bool init();

// Battery voltage in millivolts, averaged over 16 conversions. Returns 0 if the ADC fails.
uint32_t read_mv();

// Rough charge left (0..100, in steps of 5) for four alkaline AA cells in series, from the pack
// voltage. Follows a typical alkaline discharge curve at light load, so read it before the LEDs
// and speaker draw much current. 4.0 V (1.0 V per cell) is 0%: the planned cutoff.
// NiMH cells sit at 1.2-1.3 V for most of their charge, so they would read low (25-55% when full).
uint8_t percent_from_mv(uint32_t pack_mv);

}  // namespace battery
