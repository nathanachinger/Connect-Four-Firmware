#pragma once

#include <cstdint>

#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"

namespace menu {

enum class Choice : uint8_t { None, OnePlayer, TwoPlayer, Settings };

// Main menu: turn to move the highlight, click to choose. Shows three items at a time and scrolls
// when there are more. The selection is kept between visits.
class Menu {
public:
    Menu(led::Matrix& matrix, lcd::Display& screen) : matrix_(matrix), screen_(screen) {}

    // Draws the menu. Call when entering it.
    void start();
    // Returns the item the player clicked, or Choice::None.
    Choice update(const input::Events& in);

private:
    void render_screen();
    void render_leds();

    led::Matrix& matrix_;
    lcd::Display& screen_;
    uint8_t selected_ = 1;  // 2 PLAYER until the bot exists
    uint8_t top_ = 0;       // first visible item
};

}  // namespace menu
