#pragma once

#include <cstdint>

#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"

namespace menu {

enum class Choice : uint8_t { None, Back, OnePlayer, TwoPlayer, Settings, Easy, Medium, Hard };

struct Item {
    const char* label;  // up to 19 characters
    Choice choice;
    bool enabled;       // disabled items show but can't be chosen
};

// A list menu: turn to move the highlight, click to choose, hold to go back. Shows three items
// at a time and scrolls when there are more. The selection is kept between visits.
class Menu {
public:
    // `items` must outlive the menu. `hint` is drawn on the bottom line.
    Menu(led::Matrix& matrix, lcd::Display& screen, const Item* items, uint8_t count,
         uint8_t initial, const char* hint)
        : matrix_(matrix), screen_(screen), items_(items), count_(count), hint_(hint),
          selected_(initial) {}

    // Draws the menu. Call when entering it.
    void start();
    // Returns the clicked item's choice, Choice::Back on hold, or Choice::None. Plays the move,
    // select and blocked sounds; the caller plays the back sound if it acts on Choice::Back.
    Choice update(const input::Events& in, uint32_t now_ms);

private:
    void render_screen();
    void render_leds();

    led::Matrix& matrix_;
    lcd::Display& screen_;
    const Item* items_;
    uint8_t count_;
    const char* hint_;
    uint8_t selected_;
    uint8_t top_ = 0;  // first visible item
};

}  // namespace menu
