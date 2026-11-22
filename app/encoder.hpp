#pragma once

#include <cstdint>

namespace encoder {

// PEC11R rotary encoder: A/B on TIM3 CH1/CH2 (PA6/PA7) in encoder mode (x4 counting),
// push switch on ENCODER_SEL (PA5) with an external 10k pull-up, active low.
//
// Counts per detent depends on the part's pulses-per-rev vs detents. 4 assumes one full
// quadrature cycle per detent; check against raw counts if detents step by 2 or 1/2.
inline constexpr int32_t kCountsPerDetent = 4;

// Switches PA5 to an input (CubeMX has it as an output) and starts TIM3.
bool init();

// Raw signed count since init, widened from the 16-bit timer. Call at least every ~8k counts.
int32_t count();
inline int32_t detents() { return count() / kCountsPerDetent; }

// Call regularly (every few ms). Debounces the switch (20 ms).
void update_button();
bool button_down();
// Number of debounced presses since init.
uint32_t button_presses();

}  // namespace encoder
