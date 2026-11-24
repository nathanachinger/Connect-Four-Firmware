#pragma once

#include <cstdint>

#include "board.hpp"
#include "bot.hpp"
#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"
#include "menu.hpp"
#include "settings.hpp"
#include "sound.hpp"

namespace match {

struct Setup {
    bool vs_bot;      // false: pass and play
    bot::Level level; // used when vs_bot
};

// Clock and input, injected so the game logic also runs in host-side tests.
struct Platform {
    uint32_t (*now_ms)();
    input::Events (*poll)();
};

// One game screen for both modes. Turn to aim along the top LED row, click to drop. Against the
// bot, the player is red and the bot is yellow; who starts alternates each game, player first.
// After a game, click to play again.
//
// Hold (any time, also while the bot is thinking) pauses with a menu over the frozen board:
//   RESUME
//   GET HINT  on the player's turn: until they drop, a piece in their color blinks quickly where
//             the best move would land
//   QUIT      back to the main menu
//
// Double-click takes back the last move (in 1 player, back to the player's last move), also
// after the game ends. So a single click acts once the double-click window has passed.
//
// Debug mode (settings) adds bot insights on the LCD.
class Game {
public:
    Game(led::Matrix& matrix, lcd::Display& screen, const Platform& platform)
        : matrix_(matrix), screen_(screen), platform_(platform),
          pause_menu_(matrix, screen, pause_items_, kPauseItems, 0, "CLICK=OK HOLD=RESUME") {}

    // `seed` randomizes the bot (e.g. ADC noise mixed with the time the player took in the menu).
    void start(const Setup& setup, uint32_t now_ms, uint32_t seed);
    // Call every loop iteration with fresh input. Returns false when the player quits from the
    // pause menu. Blocks for up to bot::kHardBudgetMs while the bot thinks.
    bool update(const input::Events& in, uint32_t now_ms);

    // Last search, for checking bot speed in the debugger.
    const bot::Stats& last_bot_stats() const { return bot_stats_; }

private:
    // Aim: a person is choosing. Think: the bot is searching (next update blocks).
    // BotAim: thinking animation finishes, then the bot's piece slides to its column.
    enum class State : uint8_t { Aim, Think, BotAim, Drop, Over };

    static constexpr game::Player kBotPlayer = game::Player::Yellow;

    bool bot_turn() const { return setup_.vs_bot && board_.to_move() == kBotPlayer; }

    void new_game(game::Player first, uint32_t now_ms);
    void begin_turn(uint32_t now_ms);
    void show_cursor(uint32_t now_ms);
    void start_drop(uint8_t col, uint32_t now_ms);

    void aim(const input::Events& in, uint32_t now_ms);
    void think(uint32_t now_ms);
    void bot_aim(uint32_t now_ms);
    void drop(uint32_t now_ms);
    void over(const input::Events& in, uint32_t now_ms);
    void undo(uint32_t now_ms);
    // Moves left after an undo, or -1 if there is nothing (of the player's) to take back.
    int8_t undo_target() const;

    void pause(uint32_t now_ms);
    void resume(uint32_t now_ms);
    bool update_paused(const input::Events& in, uint32_t now_ms);
    game::Player mover_of(uint8_t move) const {
        return move % 2 == 0 ? first_ : game::other(first_);
    }

    // Moves the "thinking" piece one step along the top row, bouncing at the ends.
    void step_scan();
    static bool on_search_progress(void* ctx);

    sound::Id result_sound() const;
    void render_debug();
    void render_columns(uint8_t line);
    void render_leds();
    void render_screen();

    led::Matrix& matrix_;
    lcd::Display& screen_;
    Platform platform_;

    Setup setup_ = {false, bot::Level::Easy};
    game::Board board_;
    game::Player first_ = game::Player::Red;
    State state_ = State::Aim;

    uint8_t aim_col_ = game::kCols / 2;  // the person's cursor; kept across the bot's turns
    uint8_t bot_col_ = game::kCols / 2;  // the bot's piece while it thinks and slides
    int8_t scan_dir_ = 1;
    uint8_t bot_target_ = 0;
    uint32_t think_start_ms_ = 0;
    uint32_t next_scan_ms_ = 0;
    bool pause_requested_ = false;  // hold while the bot was searching

    bot::Rng rng_;
    bot::Stats bot_stats_ = {};
    bool have_bot_stats_ = false;  // bot_stats_ describe the bot's latest move on the board

    // Columns played this game, in order, for undo.
    uint8_t history_[game::kCols * game::kRows] = {};
    uint8_t history_len_ = 0;

    // Every column scored for the person about to move (hint and debug).
    bot::Analysis analysis_ = {};
    bool hint_ = false;          // hint for this turn
    bool hint_on_ = false;       // hint piece visible (it blinks)
    uint32_t next_hint_ms_ = 0;  // next hint blink

    // A click waiting to see whether a second one makes it a double-click.
    bool pending_click_ = false;
    uint32_t pending_until_ms_ = 0;

    static constexpr uint8_t kPauseItems = 3;
    menu::Item pause_items_[kPauseItems] = {
        {"RESUME", menu::Choice::Resume, true},
        {"GET HINT", menu::Choice::Hint, true},
        {"QUIT", menu::Choice::Quit, true},
    };
    menu::Menu pause_menu_;
    bool paused_ = false;
    uint32_t pause_start_ms_ = 0;

    // Falling piece: column, current and final LED row, and which player dropped it.
    uint8_t drop_col_ = 0;
    uint8_t drop_led_row_ = 0;
    uint8_t drop_target_ = 0;
    game::Player drop_player_ = game::Player::Red;

    uint32_t next_step_ms_ = 0;  // next cursor blink, slide step, drop step or highlight blink
    bool blink_on_ = false;      // cursor visible (Aim) or winning line visible (Over)

    uint8_t shown_pwm_ = 0;  // LED brightness of the last render
    bool leds_dirty_ = true;
    bool screen_dirty_ = true;
};

}  // namespace match
