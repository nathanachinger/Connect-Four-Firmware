#pragma once

#include <cstdint>

#include "board.hpp"
#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"

namespace two_player {

// Pass-and-play: both players share the knob. Turn to aim along the top LED row, click to drop.
// After a game, click to play again (the other player starts). Hold restarts at any time.
class Game {
public:
    Game(led::Matrix& matrix, lcd::Display& screen) : matrix_(matrix), screen_(screen) {}

    void start(uint32_t now_ms);
    // Call every loop iteration with fresh input.
    void update(const input::Events& in, uint32_t now_ms);

private:
    enum class State : uint8_t { Aim, Drop, Over };

    void new_game(game::Player first, uint32_t now_ms);
    void show_cursor(uint32_t now_ms);
    void aim(const input::Events& in, uint32_t now_ms);
    void drop(uint32_t now_ms);
    void over(const input::Events& in, uint32_t now_ms);

    void render_leds();
    void render_screen();

    led::Matrix& matrix_;
    lcd::Display& screen_;

    game::Board board_;
    game::Player first_ = game::Player::Red;
    State state_ = State::Aim;
    uint8_t cursor_ = game::kCols / 2;

    // Falling piece: column, current and final LED row, and which player dropped it.
    uint8_t drop_col_ = 0;
    uint8_t drop_led_row_ = 0;
    uint8_t drop_target_ = 0;
    game::Player drop_player_ = game::Player::Red;

    uint32_t next_step_ms_ = 0;  // next cursor blink, drop step or highlight blink
    bool blink_on_ = false;      // cursor visible (Aim) or winning line visible (Over)

    bool leds_dirty_ = true;
    bool screen_dirty_ = true;
};

}  // namespace two_player
