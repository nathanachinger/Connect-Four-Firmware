#include "app_main.h"

#include "backlight.hpp"
#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"
#include "light_sensor.hpp"
#include "match.hpp"
#include "menu.hpp"
#include "peripherals.hpp"

namespace {

constexpr uint8_t kBacklightPercent = 80;
constexpr uint32_t kLoopMs = 2;  // input poll rate; well under the 20 ms debounce

constexpr menu::Item kMainItems[] = {
    {"1 PLAYER", menu::Choice::OnePlayer, true},
    {"2 PLAYER", menu::Choice::TwoPlayer, true},
    {"SETTINGS  (SOON)", menu::Choice::Settings, false},
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

}  // namespace

extern "C" void app_main(void) {
    static lcd::Display screen;
    static led::Matrix matrix;
    static menu::Menu main_menu(matrix, screen, kMainItems, 3, 1, "TURN=SELECT CLICK=OK");
    static menu::Menu level_menu(matrix, screen, kLevelItems, 3, 1, "CLICK=OK  HOLD=BACK");
    static match::Game game(matrix, screen, match::Platform{HAL_GetTick, poll_input});

    backlight::init();
    backlight::set_percent(kBacklightPercent);
    screen.init();
    wait_for_leds(matrix, screen);
    input::init();
    light::init();

    enum class Mode : uint8_t { MainMenu, LevelMenu, Match };
    Mode mode = Mode::MainMenu;
    main_menu.start();

    for (;;) {
        const input::Events in = input::poll();
        const uint32_t now = HAL_GetTick();
        switch (mode) {
            case Mode::MainMenu:
                switch (main_menu.update(in)) {
                    case menu::Choice::OnePlayer:
                        level_menu.start();
                        mode = Mode::LevelMenu;
                        break;
                    case menu::Choice::TwoPlayer:
                        game.start({false, bot::Level::Easy}, now, 0);
                        mode = Mode::Match;
                        break;
                    default:
                        break;
                }
                break;

            case Mode::LevelMenu: {
                const menu::Choice choice = level_menu.update(in);
                if (choice == menu::Choice::Back) {
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
                    main_menu.start();
                    mode = Mode::MainMenu;
                }
                break;
        }
        HAL_Delay(kLoopMs);
    }
}
