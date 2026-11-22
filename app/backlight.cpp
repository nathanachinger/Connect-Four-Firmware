#include "backlight.hpp"

#include "peripherals.hpp"

namespace backlight {
namespace {

uint8_t current = 0;

}  // namespace

bool init() {
    // Also sets MOE, which TIM1 (an advanced timer) needs before its outputs drive the pin.
    return HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) == HAL_OK;
}

void set_percent(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    current = percent;
    const uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1;
    const uint32_t duty = period * percent * percent / 10000u;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty);
}

uint8_t percent() { return current; }

}  // namespace backlight
