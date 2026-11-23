#pragma once

#include <cstdint>

namespace encoder {

// PEC11R rotary encoder: A/B on TIM3 CH1/CH2 (PA6/PA7) in encoder mode (x4 counting),
// push switch on ENCODER_SEL (PA5) with an external 10k pull-up, active low.
//
// Counts per detent depends on the part's pulses-per-rev vs detents. This PEC11R variant has
// half a quadrature cycle per detent (e.g. 12 PPR / 24 detents), so x4 counting gives 2.
inline constexpr int32_t kCountsPerDetent = 2;

// Starts TIM3. The timer and pins are configured in CubeMX.
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
