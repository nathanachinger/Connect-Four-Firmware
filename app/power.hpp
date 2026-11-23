#pragma once

#include "display.hpp"
#include "led_matrix.hpp"

namespace power {

// Call once at boot: drops any Shutdown-mode pin pulls left from before, so they never apply
// while running.
void init();

// Turns off the speaker, LED board, display and backlight, then puts the STM32 in Shutdown mode
// (its lowest-power state) with no wakeup source. Only a power cycle or reset starts it again.
// The buck stays on, so the floor current is the battery divider and the buck's quiescent draw.
[[noreturn]] void shut_down(led::Matrix& matrix, lcd::Display& screen);

}  // namespace power
