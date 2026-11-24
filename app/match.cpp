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
// Hint piece blinks faster than the cursor, so the two are easy to tell apart.
constexpr uint32_t kHintBlinkMs = 150;

// Debug: depth of the search that scores each column for the person about to move.
constexpr uint8_t kEvalDepth = 5;

// Second click within this long of the first = undo.
constexpr uint32_t kDoubleClickMs = 350;

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

// "D9 N63186 900MS": depth, positions searched, time.
char* put_stats(char* p, uint8_t depth, uint32_t nodes, uint32_t ms) {
    p = put_count(put(put(put(p, "D"), depth), " N"), nodes);
    return put(put(put(p, " "), ms), "MS");
}

// "PV 4 3 4 5": an expected line, columns numbered 1-7 from the left.
char* put_pv(char* p, const uint8_t* pv, uint8_t len) {
    if (len == 0) {
        return p;
    }
    p = put(p, "PV");
    for (uint8_t i = 0; i < len; ++i) {
        p = put(put(p, " "), static_cast<uint32_t>(pv[i] + 1));
    }
    return p;
}

// Who a score favors, in words. `score` is from side A's point of view.
struct Side {
    const char* wins;   // "BOT WINS IN "
    const char* ahead;  // "BOT AHEAD BY "
};
constexpr Side kYou = {"YOU WIN IN ", "YOU'RE AHEAD BY "};
constexpr Side kBot = {"BOT WINS IN ", "BOT AHEAD BY "};
constexpr Side kRed = {"RED WINS IN ", "RED AHEAD BY "};
constexpr Side kYellow = {"YELLOW WINS IN ", "YELLOW AHEAD BY "};

// A forced result reads "... WINS IN n PLY", n counting half-moves from the searched position.
char* put_ahead(char* p, int32_t score, const Side& a, const Side& b) {
    const Side& leader = score > 0 ? a : b;
    const auto magnitude = static_cast<uint32_t>(score >= 0 ? score : -score);
    if (magnitude >= static_cast<uint32_t>(bot::kWinThreshold)) {
        return put(put(put(p, leader.wins), bot::kWinScore - magnitude), " PLY");
    }
    if (magnitude == 0) {
        return put(p, "EVEN");
    }
    return put(put(p, leader.ahead), magnitude);
}

bool due(uint32_t now_ms, uint32_t at_ms) { return static_cast<int32_t>(now_ms - at_ms) >= 0; }

}  // namespace

void Game::start(const Setup& setup, uint32_t now_ms, uint32_t seed) {
    setup_ = setup;
    rng_.reseed(seed);
    aim_col_ = game::kCols / 2;
    have_bot_stats_ = false;
    paused_ = false;  // left by QUIT
    new_game(Player::Red, now_ms);
}

void Game::new_game(Player first, uint32_t now_ms) {
    first_ = first;
    board_.reset(first);
    history_len_ = 0;
    pending_click_ = false;
    hint_ = false;  // a hint never carries into another game
    have_bot_stats_ = false;
    pause_requested_ = false;
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
        if (settings::values().debug) {
            bot::analyze(board_, kEvalDepth, analysis_);
        }
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
    history_[history_len_++] = col;
    hint_ = false;  // a hint lasts one turn
    pending_click_ = false;
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

bool Game::update(const input::Events& event, uint32_t now_ms) {
    if (paused_) {
        return update_paused(event, now_ms);
    }
    if (event.gesture == input::Gesture::Hold) {
        pending_click_ = false;
        pause(now_ms);
        return true;
    }
    input::Events in = event;
    if (state_ != State::Aim && state_ != State::Over) {
        pending_click_ = false;
    } else {
        // Hold each click back until it's clear it isn't the first of a double-click.
        if (in.gesture == input::Gesture::Click) {
            in.gesture = input::Gesture::None;
            if (pending_click_) {
                pending_click_ = false;
                undo(now_ms);
            } else {
                pending_click_ = true;
                pending_until_ms_ = now_ms + kDoubleClickMs;
            }
        } else if (pending_click_ && due(now_ms, pending_until_ms_)) {
            pending_click_ = false;
            in.gesture = input::Gesture::Click;
        }
        if (pending_click_) {
            in.steps = 0;  // drop where the click was
        }
    }
    switch (state_) {
        case State::Aim: aim(in, now_ms); break;
        case State::Think: think(now_ms); break;
        case State::BotAim: bot_aim(now_ms); break;
        case State::Drop: drop(now_ms); break;
        case State::Over: over(in, now_ms); break;
    }
    if (pause_requested_) {
        pause_requested_ = false;
        pause(now_ms);
        return true;
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
    if (hint_ && due(now_ms, next_hint_ms_)) {
        hint_on_ = !hint_on_;
        next_hint_ms_ = now_ms + kHintBlinkMs;
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
        self->pause_requested_ = true;
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
    bot::Stats stats;
    bot_target_ = bot::choose(board_, setup_.level, rng_, hooks, &stats);
    if (pause_requested_) {
        return;  // search abandoned; it starts over after the pause
    }
    bot_stats_ = stats;
    have_bot_stats_ = true;
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

int8_t Game::undo_target() const {
    uint8_t n = history_len_;
    if (setup_.vs_bot) {
        // Back to the player's last move, so it's their turn again.
        while (n > 0 && mover_of(static_cast<uint8_t>(n - 1)) == kBotPlayer) {
            --n;
        }
    }
    return static_cast<int8_t>(n - 1);
}

void Game::undo(uint32_t now_ms) {
    const int8_t target = undo_target();
    if (target < 0) {
        sound::play(sound::Id::Blocked, now_ms);  // nothing (of the player's) to take back
        return;
    }
    const auto n = static_cast<uint8_t>(target);
    aim_col_ = history_[n];  // the cursor goes back to where the piece was
    history_len_ = n;
    board_.reset(first_);
    for (uint8_t i = 0; i < n; ++i) {
        board_.play(history_[i]);
    }
    have_bot_stats_ = false;  // they described a move that's gone now
    hint_ = false;
    sound::play(sound::Id::MenuBack, now_ms);
    begin_turn(now_ms);
    leds_dirty_ = true;
}

void Game::pause(uint32_t now_ms) {
    paused_ = true;
    pause_start_ms_ = now_ms;
    pause_items_[1].enabled = state_ == State::Aim && !hint_;  // the player's turn, no hint yet
    pause_menu_.select(0);
    sound::play(sound::Id::MenuSelect, now_ms);
    // The board stays up, frozen; an aiming piece is shown solid.
    blink_on_ = true;
    render_leds();
    leds_dirty_ = false;
    pause_menu_.start(true);
}

void Game::resume(uint32_t now_ms) {
    paused_ = false;
    // Pick up animations where they stopped.
    const uint32_t paused_for = now_ms - pause_start_ms_;
    think_start_ms_ += paused_for;
    next_scan_ms_ += paused_for;
    next_step_ms_ += paused_for;
    next_hint_ms_ += paused_for;
    if (state_ == State::Aim) {
        show_cursor(now_ms);
    }
    leds_dirty_ = true;
    screen_dirty_ = true;
}

bool Game::update_paused(const input::Events& in, uint32_t now_ms) {
    switch (pause_menu_.update(in, now_ms)) {
        case menu::Choice::Back:
            sound::play(sound::Id::MenuBack, now_ms);
            resume(now_ms);
            break;
        case menu::Choice::Resume: resume(now_ms); break;
        case menu::Choice::Hint:  // only offered on the player's turn
            if (!settings::values().debug) {
                bot::analyze(board_, kEvalDepth, analysis_);  // debug keeps it up to date anyway
            }
            resume(now_ms);
            hint_ = true;
            hint_on_ = true;
            next_hint_ms_ = now_ms + kHintBlinkMs;
            break;
        case menu::Choice::Quit: return false;
        default:
            if (settings::led_pwm() != shown_pwm_) {
                render_leds();  // auto brightness keeps working while paused
            }
            break;
    }
    return true;
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
            if (hint_ && hint_on_) {
                // Where the best move would land (the column can't be full: it's playable).
                const uint8_t col = analysis_.best_col;
                matrix_.set(led_row(board_.height(col)), col, color_of(board_.to_move()), pwm);
            }
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

// Debug view, the whole screen while a game is on.
//   2 player:        turn and move number / who is ahead / expected line / column scores
//   1 player, you:   who is ahead / bot's last search / its expected line / column scores
//   1 player, bot:   title / THINKING... / (blank) / who is ahead, from its previous search
//                    (nothing changes until its piece lands)
// Who is ahead comes from the bot's last search in 1 player (from the position it searched, a
// forced result counting its own move), otherwise from the column scores. Column scores rate
// each column for the side to move by a kEvalDepth search: Wn / Ln = forced win / loss in n ply,
// "." = full; the best is highlighted.
void Game::render_debug() {
    char line[24];
    if (!setup_.vs_bot) {
        const bool red = board_.to_move() == Player::Red;
        put(put(put(line, red ? "RED" : "YELLOW"), "'S TURN #"),
            static_cast<uint32_t>(board_.moves() + 1));
        centered(screen_, 0, line);
        put_ahead(line, red ? analysis_.best : -analysis_.best, kRed, kYellow);
        centered(screen_, 1, line);
        put_pv(line, analysis_.pv, analysis_.pv_len);
        centered(screen_, 2, line);
        render_columns(3);
        return;
    }

    const bot::Stats& st = bot_stats_;
    const bool searched = have_bot_stats_ && setup_.level != bot::Level::Easy;
    // Line with the bot's search (or why there is none).
    auto stats_line = [&](uint8_t at) {
        if (!have_bot_stats_) {
            centered(screen_, at, "NO BOT SEARCH");
        } else if (setup_.level == bot::Level::Easy) {
            centered(screen_, at, "EASY: NO SEARCH");
        } else {
            put_stats(line, st.depth, st.nodes, st.ms);
            centered(screen_, at, line);
        }
    };

    if (bot_turn()) {
        std::strcpy(line, "1 PLAYER - ");
        std::strcat(line, level_name(setup_.level));
        centered(screen_, 0, line);
        centered(screen_, 1, "THINKING...");
        if (searched) {
            put_ahead(line, st.score, kBot, kYou);
            centered(screen_, 3, line);
        }
        return;
    }

    if (searched) {
        put_ahead(line, st.score, kBot, kYou);
    } else {
        put_ahead(line, analysis_.best, kYou, kBot);
    }
    centered(screen_, 0, line);
    stats_line(1);
    if (searched) {
        put_pv(line, st.pv, st.pv_len);
        centered(screen_, 2, line);
    }
    render_columns(3);
}

// Seven 18 px cells, one per column, e.g. " 12 -3  W3  .  -L4". The best is shown inverted.
void Game::render_columns(uint8_t at) {
    constexpr uint8_t kCell = 18;
    constexpr uint8_t kLeft = (lcd::kWidth - kCell * game::kCols) / 2;
    for (uint8_t c = 0; c < game::kCols; ++c) {
        const int32_t s = analysis_.scores[c];
        char text[8];
        if (s == bot::kFullColumn) {
            put(text, ".");
        } else if (s >= bot::kWinThreshold) {
            put(put(text, "W"), static_cast<uint32_t>(bot::kWinScore - s));
        } else if (s <= -bot::kWinThreshold) {
            put(put(text, "L"), static_cast<uint32_t>(bot::kWinScore + s));
        } else {
            const int32_t v = s > 99 ? 99 : (s < -99 ? -99 : s);
            if (v < 0) {
                put(put(text, "-"), static_cast<uint32_t>(-v));
            } else {
                put(text, static_cast<uint32_t>(v));
            }
        }
        const auto width = static_cast<uint8_t>(std::strlen(text) * lcd::kCharWidth);
        const auto x = static_cast<uint8_t>(kLeft + c * kCell);
        screen_.text(static_cast<uint8_t>(x + (kCell - width + 1) / 2), at, text);
        if (s == analysis_.best) {
            screen_.invert_line(at, x, kCell);
        }
    }
}

void Game::render_screen() {
    char line[22];
    screen_.clear();
    const bool debug = settings::values().debug;
    if (debug && !board_.over()) {
        render_debug();
        screen_.show();
        return;
    }

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
        if (!bot_turn()) {  // left blank while the bot thinks
            centered(screen_, 2, "HOLD=MENU 2X=UNDO");
            centered(screen_, 3, "TURN=AIM  CLICK=DROP");
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
        centered(screen_, 2, "HOLD=MENU 2X=UNDO");
        centered(screen_, 3, "CLICK=PLAY AGAIN");
    }
    screen_.show();
}

}  // namespace match
