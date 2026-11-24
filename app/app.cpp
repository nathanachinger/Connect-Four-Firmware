#include "app_main.h"

#include "adc.hpp"
#include "backlight.hpp"
#include "battery.hpp"
#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"
#include "light_sensor.hpp"
#include "match.hpp"
#include "menu.hpp"
#include "peripherals.hpp"
#include "power.hpp"
#include "settings.hpp"
#include "settings_screen.hpp"
#include "sound.hpp"

namespace {

constexpr uint32_t kLoopMs = 2;  // input poll rate; well under the 20 ms debounce
constexpr uint32_t kStartupMs = 1500;  // startup screen; a click skips it
constexpr uint8_t kLowBatteryPercent = 10;
constexpr uint32_t kAutoOffMs = 5 * 60 * 1000;  // no input for this long -> shut down
constexpr uint32_t kAutoOffNoticeMs = 1500;     // "AUTO OFF" message before going dark

constexpr menu::Item kMainItems[] = {
    {"1 PLAYER", menu::Choice::OnePlayer, true},
    {"2 PLAYER", menu::Choice::TwoPlayer, true},
    {"SETTINGS", menu::Choice::Settings, true},
};

constexpr menu::Item kLevelItems[] = {
    {"EASY", menu::Choice::Easy, true},
    {"MEDIUM", menu::Choice::Medium, true},
    {"HARD", menu::Choice::Hard, true},
};

// Shows an error on the LCD and keeps retrying the LED driver until it answers.
void wait_for_leds(led::Matrix& matrix, lcd::Display& screen) {
    while (!matrix.init()) {
        screen.clear();
        screen.text(0, 0, "LED DRIVER ERROR");
        screen.text(0, 1, "CHECK I2C WIRING");
        screen.show();
        HAL_GPIO_TogglePin(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin);
        HAL_Delay(500);
    }
    HAL_GPIO_WritePin(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin, GPIO_PIN_RESET);
}

// Bot seed: low bits of single ADC conversions (noise) mixed with the millisecond the player
// clicked, so games differ even from the same position.
uint32_t random_seed() {
    uint32_t seed = HAL_GetTick();
    for (int i = 0; i < 8; ++i) {
        seed = (seed ^ light::read_raw(1)) * 0x9E3779B1u;
        seed ^= seed >> 15;
    }
    return seed;
}

input::Events poll_input() { return input::poll(); }

void centered(lcd::Display& screen, uint8_t line, const char* text) {
    uint8_t len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    const auto width = static_cast<uint32_t>(len) * lcd::kCharWidth;
    screen.text(static_cast<uint8_t>(width < lcd::kWidth ? (lcd::kWidth - width) / 2 : 0), line,
                text);
}

// Flash writes need a steady supply; skip saving once the pack is at the 4.0 V cutoff, where a
// sagging rail could cut a write short. (0 means the ADC read failed; save anyway.)
constexpr uint32_t kMinSaveMv = 4000;

void save_settings() {
    const uint32_t mv = battery::read_mv();
    if (mv == 0 || mv >= kMinSaveMv) {
        settings::save();
    }
}

bool is_activity(const input::Events& in) {
    return in.steps != 0 || in.button_down || in.gesture != input::Gesture::None;
}

// Says why it went dark, then turns everything off until the next power cycle.
[[noreturn]] void auto_off(led::Matrix& matrix, lcd::Display& screen) {
    save_settings();  // in case it timed out on the settings screen
    sound::stop();
    matrix.clear();
    matrix.show();
    screen.clear();
    centered(screen, 1, "AUTO OFF");
    centered(screen, 2, "POWER CYCLE TO WAKE");
    screen.show();
    HAL_Delay(kAutoOffNoticeMs);
    power::shut_down(matrix, screen);
}

// Title and battery estimate. Read before the LEDs light, while the pack is lightly loaded.
void show_startup(lcd::Display& screen) {
    screen.clear();
    centered(screen, 0, "CONNECT FOUR");

    uint32_t mv = 0;
    if (battery::init()) {
        adc::measure_vdda_mv();
        mv = battery::read_mv();
    }
    if (mv == 0) {
        centered(screen, 2, "BATTERY --");
    } else {
        const uint8_t pct = battery::percent_from_mv(mv);

        // Battery outline with a fill bar, then "85%" to its right; the pair is centered.
        constexpr uint8_t kBodyW = 26;
        constexpr uint8_t kBodyH = 10;
        constexpr uint8_t kNubW = 2;
        constexpr uint8_t kGap = 6;
        char text[5];
        uint8_t n = 0;
        if (pct >= 100) {
            text[n++] = '1';
        }
        if (pct >= 10) {
            text[n++] = static_cast<char>('0' + pct / 10 % 10);
        }
        text[n++] = static_cast<char>('0' + pct % 10);
        text[n++] = '%';
        text[n] = '\0';
        const auto total = static_cast<uint8_t>(kBodyW + kNubW + kGap + n * lcd::kCharWidth);
        const auto x = static_cast<uint8_t>((lcd::kWidth - total) / 2);
        constexpr uint8_t y = 14;  // rows 14-23: level with line 2, clear of line 3
        screen.draw_rect(x, y, kBodyW, kBodyH);
        screen.fill_rect(static_cast<uint8_t>(x + kBodyW), y + 3, kNubW, kBodyH - 6, true);
        const auto fill = static_cast<uint8_t>((kBodyW - 4) * pct / 100);
        screen.fill_rect(static_cast<uint8_t>(x + 2), y + 2, fill, kBodyH - 4, true);
        screen.text(static_cast<uint8_t>(x + kBodyW + kNubW + kGap), 2, text);

        if (pct <= kLowBatteryPercent) {
            centered(screen, 3, "LOW BATTERY");
        }
    }
    screen.show();

    const uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < kStartupMs) {
        if (input::poll().gesture == input::Gesture::Click) {
            break;
        }
        HAL_Delay(kLoopMs);
    }
}

}  // namespace

extern "C" void app_main(void) {
    static lcd::Display screen;
    static led::Matrix matrix;
    static menu::Menu main_menu(matrix, screen, kMainItems, 3, 1, "TURN=SELECT CLICK=OK");
    static menu::Menu level_menu(matrix, screen, kLevelItems, 3, 1, "CLICK=OK  HOLD=BACK");
    static match::Game game(matrix, screen, match::Platform{HAL_GetTick, poll_input});
    static settings_screen::Screen settings_page(matrix, screen);

    power::init();
    backlight::init();
    light::init();
    settings::load();
    settings::init();
    settings::update(HAL_GetTick());  // backlight on at the saved or room-matched level
    screen.init();
    input::init();
    show_startup(screen);
    wait_for_leds(matrix, screen);

    enum class Mode : uint8_t { MainMenu, LevelMenu, Match, Settings };
    Mode mode = Mode::MainMenu;
    main_menu.start();
    uint32_t last_input_ms = HAL_GetTick();

    for (;;) {
        const input::Events in = input::poll();
        const uint32_t now = HAL_GetTick();
        if (is_activity(in)) {
            last_input_ms = now;
        } else if (now - last_input_ms >= kAutoOffMs) {
            auto_off(matrix, screen);
        }
        switch (mode) {
            case Mode::MainMenu:
                switch (main_menu.update(in, now)) {
                    case menu::Choice::OnePlayer:
                        level_menu.start();
                        mode = Mode::LevelMenu;
                        break;
                    case menu::Choice::TwoPlayer:
                        game.start({false, bot::Level::Easy}, now, 0);
                        mode = Mode::Match;
                        break;
                    case menu::Choice::Settings:
                        settings_page.start();
                        mode = Mode::Settings;
                        break;
                    default:
                        break;
                }
                break;

            case Mode::LevelMenu: {
                const menu::Choice choice = level_menu.update(in, now);
                if (choice == menu::Choice::Back) {
                    sound::play(sound::Id::MenuBack, now);
                    main_menu.start();
                    mode = Mode::MainMenu;
                } else if (choice != menu::Choice::None) {
                    const bot::Level level = choice == menu::Choice::Easy     ? bot::Level::Easy
                                             : choice == menu::Choice::Medium ? bot::Level::Medium
                                                                              : bot::Level::Hard;
                    game.start({true, level}, now, random_seed());
                    mode = Mode::Match;
                }
                break;
            }

            case Mode::Match:
                if (!game.update(in, now)) {
                    sound::play(sound::Id::MenuBack, now);
                    main_menu.start();
                    mode = Mode::MainMenu;
                }
                break;

            case Mode::Settings:
                if (!settings_page.update(in, now)) {
                    sound::play(sound::Id::MenuBack, now);
                    save_settings();
                    main_menu.start();
                    mode = Mode::MainMenu;
                }
                break;
        }
        settings::update(now);
        sound::update(HAL_GetTick());
        HAL_Delay(kLoopMs);
    }
}
