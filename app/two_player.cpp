#include "two_player.hpp"

#include <cstring>

namespace two_player {
namespace {

using game::Player;
using game::Result;
using led::Color;

constexpr uint8_t kBrightness = 128;
constexpr uint32_t kDropStepMs = 45;  // per LED row while a piece falls
constexpr uint32_t kBlinkMs = 300;    // winning-line highlight
// Aiming piece blinks so it stands out from the board, but turns solid (and restarts the blink)
// whenever it moves, so it never looks like it vanished mid-turn.
constexpr uint32_t kCursorOnMs = 500;
constexpr uint32_t kCursorOffMs = 250;

// LED row 0 is the aiming row; board row r (0 = bottom) is LED row 6 - r.
constexpr uint8_t led_row(uint8_t board_row) { return game::kRows - board_row; }

Color color_of(Player p) { return p == Player::Red ? Color::Red : Color::Yellow; }

const char* name_of(Player p) { return p == Player::Red ? "RED" : "YELLOW"; }

void centered(lcd::Display& screen, uint8_t line, const char* text) {
    const auto width = static_cast<uint32_t>(std::strlen(text)) * lcd::kCharWidth;
    const auto x = static_cast<uint8_t>(width < lcd::kWidth ? (lcd::kWidth - width) / 2 : 0);
    screen.text(x, line, text);
}

}  // namespace

void Game::start(uint32_t now_ms) { new_game(Player::Red, now_ms); }

void Game::new_game(Player first, uint32_t now_ms) {
    first_ = first;
    board_.reset(first);
    state_ = State::Aim;
    cursor_ = game::kCols / 2;
    show_cursor(now_ms);
    screen_dirty_ = true;
}

void Game::show_cursor(uint32_t now_ms) {
    blink_on_ = true;
    next_step_ms_ = now_ms + kCursorOnMs;
    leds_dirty_ = true;
}

void Game::update(const input::Events& in, uint32_t now_ms) {
    if (in.gesture == input::Gesture::Hold) {
        new_game(first_, now_ms);
    } else {
        switch (state_) {
            case State::Aim: aim(in, now_ms); break;
            case State::Drop: drop(now_ms); break;
            case State::Over: over(in, now_ms); break;
        }
    }

    if (leds_dirty_) {
        render_leds();
        leds_dirty_ = false;
    }
    if (screen_dirty_) {
        render_screen();
        screen_dirty_ = false;
    }
}

void Game::aim(const input::Events& in, uint32_t now_ms) {
    if (in.steps != 0) {
        int32_t c = cursor_ + in.steps;
        c = c < 0 ? 0 : (c >= game::kCols ? game::kCols - 1 : c);
        if (c != cursor_) {
            cursor_ = static_cast<uint8_t>(c);
            show_cursor(now_ms);
        }
    }

    if (in.gesture == input::Gesture::Click) {
        const Player mover = board_.to_move();
        const int8_t row = board_.play(cursor_);
        if (row < 0) {
            return;  // column full
        }
        drop_col_ = cursor_;
        drop_led_row_ = 0;
        drop_target_ = led_row(static_cast<uint8_t>(row));
        drop_player_ = mover;
        next_step_ms_ = now_ms + kDropStepMs;
        state_ = State::Drop;
        leds_dirty_ = true;
        // A winning move keeps the mover's turn on screen until the piece lands.
        screen_dirty_ = !board_.over();
        return;
    }

    if (static_cast<int32_t>(now_ms - next_step_ms_) >= 0) {
        blink_on_ = !blink_on_;
        next_step_ms_ = now_ms + (blink_on_ ? kCursorOnMs : kCursorOffMs);
        leds_dirty_ = true;
    }
}

void Game::drop(uint32_t now_ms) {
    if (static_cast<int32_t>(now_ms - next_step_ms_) < 0) {
        return;
    }
    next_step_ms_ += kDropStepMs;
    ++drop_led_row_;
    leds_dirty_ = true;

    if (drop_led_row_ >= drop_target_) {
        if (board_.over()) {
            state_ = State::Over;
            blink_on_ = true;
            next_step_ms_ = now_ms + kBlinkMs;
            screen_dirty_ = true;
        } else {
            state_ = State::Aim;
            show_cursor(now_ms);
        }
    }
}

void Game::over(const input::Events& in, uint32_t now_ms) {
    if (in.gesture == input::Gesture::Click) {
        new_game(game::other(first_), now_ms);
        return;
    }
    if (board_.result() != Result::Draw && static_cast<int32_t>(now_ms - next_step_ms_) >= 0) {
        next_step_ms_ += kBlinkMs;
        blink_on_ = !blink_on_;
        leds_dirty_ = true;
    }
}

void Game::render_leds() {
    matrix_.clear();

    const bool dropping = state_ == State::Drop;
    // Winning pieces blink on and off in their own color. (Red + yellow together was meant to
    // read as orange, but the red LED dominates and it looks red.)
    const uint64_t hidden = (state_ == State::Over && !blink_on_) ? board_.winning_cells() : 0;

    for (uint8_t col = 0; col < game::kCols; ++col) {
        for (uint8_t row = 0; row < game::kRows; ++row) {
            const game::Cell cell = board_.at(row, col);
            if (cell == game::Cell::Empty) {
                continue;
            }
            const uint8_t r = led_row(row);
            if (dropping && col == drop_col_ && r == drop_target_) {
                continue;  // still falling; drawn below
            }
            if (hidden & game::bit(row, col)) {
                continue;
            }
            matrix_.set(r, col, cell == game::Cell::Red ? Color::Red : Color::Yellow, kBrightness);
        }
    }

    if (dropping) {
        matrix_.set(drop_led_row_, drop_col_, color_of(drop_player_), kBrightness);
    } else if (state_ == State::Aim && blink_on_) {
        matrix_.set(0, cursor_, color_of(board_.to_move()), kBrightness);
    }
    matrix_.show();
}

void Game::render_screen() {
    char line[22];
    screen_.clear();
    centered(screen_, 0, "TWO PLAYER");

    switch (board_.result()) {
        case Result::InProgress:
            std::strcpy(line, name_of(board_.to_move()));
            std::strcat(line, "'S TURN");
            centered(screen_, 1, line);
            centered(screen_, 3, "TURN=AIM  CLICK=DROP");
            break;
        case Result::RedWins:
        case Result::YellowWins:
            std::strcpy(line, board_.result() == Result::RedWins ? "RED" : "YELLOW");
            std::strcat(line, " WINS!");
            centered(screen_, 1, line);
            centered(screen_, 3, "CLICK: PLAY AGAIN");
            break;
        case Result::Draw:
            centered(screen_, 1, "DRAW!");
            centered(screen_, 3, "CLICK: PLAY AGAIN");
            break;
    }
    screen_.show();
}

}  // namespace two_player
