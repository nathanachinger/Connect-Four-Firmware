// Composition root: the only place that knows which concrete hal implementations back the App.

#include "app_main.h"

#include "app.hpp"
#include "main.h"
#include "stm32_clock.hpp"
#include "stm32_gpio.hpp"

extern "C" void app_main(void) {
    // Function-local statics: constructed here, after CubeMX init, with static storage (no heap).
    static board::Stm32Clock clock;
    static board::Stm32GpioOut low_bat_led(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin);

    static app::App app(low_bat_led, clock);

    for (;;) {
        app.tick();
    }
}
