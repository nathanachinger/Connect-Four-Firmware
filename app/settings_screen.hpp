#pragma once

#include <cstdint>

#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"

namespace settings_screen {

// Brightness (AUTO or 1-10) and sound effects (ON or OFF).
// Turn to pick a setting. Brightness: click to edit, turn to change, click to finish. Sound
// effects: click switches it. Hold goes back. Changes apply immediately: the bottom LED row lights
// as a brightness sample, and switching sound on plays a chirp.
class Screen {
public:
    Screen(led::Matrix& matrix, lcd::Display& screen) : matrix_(matrix), screen_(screen) {}

    void start();
    // Returns false when the player holds the button to go back.
    bool update(const input::Events& in, uint32_t now_ms);

private:
    void adjust(int32_t steps, uint32_t now_ms);

    void render_screen();
    void render_leds();

    led::Matrix& matrix_;
    lcd::Display& screen_;
    uint8_t selected_ = 0;  // 0 = brightness, 1 = sound effects
    bool editing_ = false;
    uint8_t shown_pwm_ = 0;
};

}  // namespace settings_screen
