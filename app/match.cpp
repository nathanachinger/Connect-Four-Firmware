#include "match.hpp"

#include <cstring>

#include "settings.hpp"
#include "sound.hpp"

namespace match {
namespace {

using game::Player;
using game::Result;
using led::Color;

constexpr uint32_t kDropStepMs = 45;  // per LED row while a piece falls
constexpr uint32_t kBlinkMs = 300;    // winning-line highlight
// Aiming piece blinks so it stands out from the board, but turns solid (and restarts the blink)
// whenever it moves, so it never looks like it vanished mid-turn.
constexpr uint32_t kCursorOnMs = 500;
constexpr uint32_t kCursorOffMs = 250;

// Bot pacing, so its moves are easy to follow even when the search is instant.
constexpr uint32_t kScanStepMs = 110;   // "thinking" piece sweeping the top row
constexpr uint32_t kMinThinkMs = 700;   // shortest visible think
constexpr uint32_t kSlideStepMs = 90;   // piece sliding to the chosen column
constexpr uint32_t kPauseMs = 250;      // hold above the column before dropping

// LED row 0 is the aiming row; board row r (0 = bottom) is LED row 6 - r.
constexpr uint8_t led_row(uint8_t board_row) { return game::kRows - board_row; }

Color color_of(Player p) { return p == Player::Red ? Color::Red : Color::Yellow; }

const char* level_name(bot::Level level) {
    switch (level) {
        case bot::Level::Easy: return "EASY";
        case bot::Level::Medium: return "MEDIUM";
        case bot::Level::Hard: return "HARD";
    }
    return "";
}

void centered(lcd::Display& screen, uint8_t line, const char* text) {
    const auto width = static_cast<uint32_t>(std::strlen(text)) * lcd::kCharWidth;
    const auto x = static_cast<uint8_t>(width < lcd::kWidth ? (lcd::kWidth - width) / 2 : 0);
    screen.text(x, line, text);
}

// Small formatting helpers for the debug lines. Each writes at p, terminates, returns the end.
char* put(char* p, const char* s) {
    while (*s != '\0') {
        *p++ = *s++;
    }
    *p = '\0';
    return p;
}
char* put(char* p, uint32_t v) {
    char digits[10];
    uint8_t n = 0;
    do {
        digits[n++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v != 0);
    while (n > 0) {
        *p++ = digits[--n];
    }
    *p = '\0';
    return p;
}
// Up to 99999 exactly, then in thousands ("1234K") so it fits.
char* put_count(char* p, uint32_t v) { return v < 100000 ? put(p, v) : put(put(p, v / 1000), "K"); }

bool due(uint32_t now_ms, uint32_t at_ms) { return static_cast<int32_t>(now_ms - at_ms) >= 0; }

}  // namespace

void Game::start(const Setup& setup, uint32_t now_ms, uint32_t seed) {
    setup_ = setup;
    rng_.reseed(seed);
    aim_col_ = game::kCols / 2;
    have_bot_stats_ = false;
    new_game(Player::Red, now_ms);
}

void Game::new_game(Player first, uint32_t now_ms) {
    first_ = first;
    board_.reset(first);
    exit_requested_ = false;
    begin_turn(now_ms);
    screen_dirty_ = true;
}

void Game::begin_turn(uint32_t now_ms) {
    if (bot_turn()) {
        state_ = State::Think;
        bot_col_ = aim_col_;
        scan_dir_ = bot_col_ < game::kCols / 2 ? 1 : -1;
        think_start_ms_ = now_ms;
        next_scan_ms_ = now_ms + kScanStepMs;
        leds_dirty_ = true;
    } else {
        state_ = State::Aim;
        show_cursor(now_ms);
    }
    screen_dirty_ = true;
}

void Game::show_cursor(uint32_t now_ms) {
    blink_on_ = true;
    next_step_ms_ = now_ms + kCursorOnMs;
    leds_dirty_ = true;
}

void Game::start_drop(uint8_t col, uint32_t now_ms) {
    const Player mover = board_.to_move();
    const int8_t row = board_.play(col);
    if (row < 0) {
        return;
    }
    drop_col_ = col;
    drop_led_row_ = 0;
    drop_target_ = led_row(static_cast<uint8_t>(row));
    drop_player_ = mover;
    next_step_ms_ = now_ms + kDropStepMs;
    state_ = State::Drop;
    leds_dirty_ = true;
    sound::play(sound::Id::Drop, now_ms);
    // A winning move keeps the current turn on screen until the piece lands.
    screen_dirty_ = !board_.over();
}

bool Game::update(const input::Events& in, uint32_t now_ms) {
    if (in.gesture == input::Gesture::Hold) {
        return false;
    }
    switch (state_) {
        case State::Aim: aim(in, now_ms); break;
        case State::Think: think(now_ms); break;
        case State::BotAim: bot_aim(now_ms); break;
        case State::Drop: drop(now_ms); break;
        case State::Over: over(in, now_ms); break;
    }
    if (exit_requested_) {
        return false;
    }

    // Brightness can change by itself in auto mode.
    if (leds_dirty_ || settings::led_pwm() != shown_pwm_) {
        render_leds();
        leds_dirty_ = false;
    }
    if (screen_dirty_) {
        render_screen();
        screen_dirty_ = false;
    }
    return true;
}

void Game::aim(const input::Events& in, uint32_t now_ms) {
    if (in.steps != 0) {
        int32_t c = aim_col_ + in.steps;
        c = c < 0 ? 0 : (c >= game::kCols ? game::kCols - 1 : c);
        if (c != aim_col_) {
            aim_col_ = static_cast<uint8_t>(c);
            show_cursor(now_ms);
            sound::play(sound::Id::Aim, now_ms);
        }
    }

    if (in.gesture == input::Gesture::Click) {
        if (board_.can_play(aim_col_)) {
            start_drop(aim_col_, now_ms);
        } else {
            sound::play(sound::Id::Blocked, now_ms);  // full column
        }
        return;
    }

    if (due(now_ms, next_step_ms_)) {
        blink_on_ = !blink_on_;
        next_step_ms_ = now_ms + (blink_on_ ? kCursorOnMs : kCursorOffMs);
        leds_dirty_ = true;
    }
}

void Game::step_scan() {
    if ((scan_dir_ > 0 && bot_col_ >= game::kCols - 1) || (scan_dir_ < 0 && bot_col_ == 0)) {
        scan_dir_ = static_cast<int8_t>(-scan_dir_);
    }
    bot_col_ = static_cast<uint8_t>(bot_col_ + scan_dir_);
    leds_dirty_ = true;
}

bool Game::on_search_progress(void* ctx) {
    auto* self = static_cast<Game*>(ctx);
    // Keep the button's debounce running; knob turns and clicks during the bot's turn are dropped.
    if (self->platform_.poll().gesture == input::Gesture::Hold) {
        self->exit_requested_ = true;
        return false;
    }
    const uint32_t now = self->platform_.now_ms();
    sound::update(now);  // the main loop is blocked while the bot searches
    if (due(now, self->next_scan_ms_)) {
        self->next_scan_ms_ = now + kScanStepMs;
        self->step_scan();
        self->render_leds();
        self->leds_dirty_ = false;
    }
    return true;
}

void Game::think(uint32_t now_ms) {
    (void)now_ms;
    bot::Hooks hooks;
    hooks.now_ms = platform_.now_ms;
    hooks.progress = &Game::on_search_progress;
    hooks.ctx = this;
    bot_target_ = bot::choose(board_, setup_.level, rng_, hooks, &bot_stats_);
    have_bot_stats_ = true;
    if (exit_requested_) {
        return;
    }
    state_ = State::BotAim;
}

void Game::bot_aim(uint32_t now_ms) {
    // Keep "thinking" until the minimum time is up, then slide over and drop.
    if (!due(now_ms, think_start_ms_ + kMinThinkMs)) {
        if (due(now_ms, next_scan_ms_)) {
            next_scan_ms_ = now_ms + kScanStepMs;
            step_scan();
        }
        next_step_ms_ = now_ms + kSlideStepMs;
        return;
    }
    if (!due(now_ms, next_step_ms_)) {
        return;
    }
    if (bot_col_ != bot_target_) {
        bot_col_ = static_cast<uint8_t>(bot_col_ < bot_target_ ? bot_col_ + 1 : bot_col_ - 1);
        next_step_ms_ = now_ms + (bot_col_ == bot_target_ ? kPauseMs : kSlideStepMs);
        leds_dirty_ = true;
        sound::play(sound::Id::Aim, now_ms);
        return;
    }
    start_drop(bot_target_, now_ms);
}

void Game::drop(uint32_t now_ms) {
    if (!due(now_ms, next_step_ms_)) {
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
            sound::play(result_sound(), now_ms);  // starts with its own landing thud
        } else {
            sound::play(sound::Id::Land, now_ms);
            begin_turn(now_ms);
        }
    }
}

void Game::over(const input::Events& in, uint32_t now_ms) {
    if (in.gesture == input::Gesture::Click) {
        sound::play(sound::Id::MenuSelect, now_ms);
        new_game(game::other(first_), now_ms);
        return;
    }
    if (board_.result() != Result::Draw && due(now_ms, next_step_ms_)) {
        next_step_ms_ += kBlinkMs;
        blink_on_ = !blink_on_;
        leds_dirty_ = true;
    }
}

sound::Id Game::result_sound() const {
    const Result result = board_.result();
    if (result == Result::Draw) {
        return sound::Id::Draw;
    }
    if (!setup_.vs_bot) {
        return sound::Id::Win;  // someone won; celebrate either color
    }
    const Result bot_wins = kBotPlayer == Player::Yellow ? Result::YellowWins : Result::RedWins;
    return result == bot_wins ? sound::Id::Lose : sound::Id::Win;
}

void Game::render_leds() {
    matrix_.clear();
    shown_pwm_ = settings::led_pwm();
    const uint8_t pwm = shown_pwm_;

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
            matrix_.set(r, col, cell == game::Cell::Red ? Color::Red : Color::Yellow, pwm);
        }
    }

    switch (state_) {
        case State::Drop:
            matrix_.set(drop_led_row_, drop_col_, color_of(drop_player_), pwm);
            break;
        case State::Aim:
            if (blink_on_) {
                matrix_.set(0, aim_col_, color_of(board_.to_move()), pwm);
            }
            break;
        case State::Think:
        case State::BotAim:
            matrix_.set(0, bot_col_, color_of(kBotPlayer), pwm);
            break;
        case State::Over:
            break;
    }
    matrix_.show();
}

// Debug view for lines 2-3, in place of the hints.
//   1 player: the bot's last search: depth, positions searched, time, and its score from its own
//             point of view (a forced result shows as WIN/LOSS IN n PLY, n counting half-moves
//             from the position it searched, its own move included).
//   2 player: the move count, then the usual hint.
void Game::render_debug() {
    char line[24];
    if (!setup_.vs_bot) {
        put(put(line, "MOVE "), static_cast<uint32_t>(board_.moves() + 1));
        centered(screen_, 2, line);
        centered(screen_, 3, "TURN=AIM  CLICK=DROP");
        return;
    }
    if (!have_bot_stats_) {
        centered(screen_, 2, "BOT: NO MOVE YET");
        return;
    }
    const bot::Stats& st = bot_stats_;
    if (setup_.level == bot::Level::Easy) {
        centered(screen_, 2, "EASY: NO SEARCH");
        put(put(line, st.ms), "MS");
        centered(screen_, 3, line);
        return;
    }
    char* p = put(put(line, "DEPTH "), st.depth);
    put_count(put(p, " NODES "), st.nodes);
    centered(screen_, 2, line);

    p = put(put(line, st.ms), "MS ");
    if (st.score >= bot::kWinThreshold || st.score <= -bot::kWinThreshold) {
        const int32_t magnitude = st.score > 0 ? st.score : -st.score;
        p = put(p, st.score > 0 ? "WIN IN " : "LOSS IN ");
        put(put(p, static_cast<uint32_t>(bot::kWinScore - magnitude)), " PLY");
    } else {
        p = put(p, st.score >= 0 ? "SCORE +" : "SCORE -");
        put(p, static_cast<uint32_t>(st.score >= 0 ? st.score : -st.score));
    }
    centered(screen_, 3, line);
}

void Game::render_screen() {
    char line[22];
    screen_.clear();

    if (setup_.vs_bot) {
        std::strcpy(line, "1 PLAYER - ");
        std::strcat(line, level_name(setup_.level));
        centered(screen_, 0, line);
    } else {
        centered(screen_, 0, "2 PLAYER");
    }

    const Result result = board_.result();
    if (result == Result::InProgress) {
        if (setup_.vs_bot) {
            centered(screen_, 1, bot_turn() ? "THINKING..." : "YOUR TURN");
        } else {
            std::strcpy(line, board_.to_move() == Player::Red ? "RED" : "YELLOW");
            std::strcat(line, "'S TURN");
            centered(screen_, 1, line);
        }
        if (settings::values().debug) {
            render_debug();
        } else {
            centered(screen_, 2, "HOLD=MENU");
            if (!bot_turn()) {
                centered(screen_, 3, "TURN=AIM  CLICK=DROP");
            }
        }
    } else {
        if (result == Result::Draw) {
            centered(screen_, 1, "DRAW!");
        } else if (setup_.vs_bot) {
            const Result bot_wins =
                kBotPlayer == Player::Yellow ? Result::YellowWins : Result::RedWins;
            centered(screen_, 1, result == bot_wins ? "BOT WINS!" : "YOU WIN!");
        } else {
            centered(screen_, 1, result == Result::RedWins ? "RED WINS!" : "YELLOW WINS!");
        }
        centered(screen_, 2, "HOLD=MENU");
        centered(screen_, 3, "CLICK=PLAY AGAIN");
    }
    screen_.show();
}

}  // namespace match
