#include "app_main.h"

#include "backlight.hpp"
#include "display.hpp"
#include "input.hpp"
#include "led_matrix.hpp"
#include "menu.hpp"
#include "peripherals.hpp"
#include "two_player.hpp"

namespace {

constexpr uint8_t kBacklightPercent = 80;
constexpr uint32_t kLoopMs = 2;  // input poll rate; well under the 20 ms debounce

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

}  // namespace

extern "C" void app_main(void) {
    static lcd::Display screen;
    static led::Matrix matrix;
    static menu::Menu main_menu(matrix, screen);
    static two_player::Game game(matrix, screen);

    backlight::init();
    backlight::set_percent(kBacklightPercent);
    screen.init();
    wait_for_leds(matrix, screen);
    input::init();

    enum class Mode : uint8_t { Menu, TwoPlayer };
    Mode mode = Mode::Menu;
    main_menu.start();

    for (;;) {
        const input::Events in = input::poll();
        const uint32_t now = HAL_GetTick();
        switch (mode) {
            case Mode::Menu:
                if (main_menu.update(in) == menu::Choice::TwoPlayer) {
                    game.start(now);
                    mode = Mode::TwoPlayer;
                }
                break;
            case Mode::TwoPlayer:
                if (!game.update(in, now)) {
                    main_menu.start();
                    mode = Mode::Menu;
                }
                break;
        }
        HAL_Delay(kLoopMs);
    }
}
