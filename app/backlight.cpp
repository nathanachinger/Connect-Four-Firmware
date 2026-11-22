#include "backlight.hpp"

#include "peripherals.hpp"

namespace backlight {
namespace {

constexpr uint32_t kPwmHz = 20000;

TIM_HandleTypeDef htim1 = {};
uint8_t current = 0;

}  // namespace

bool init() {
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM1;
    HAL_GPIO_Init(GPIOA, &gpio);

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = SystemCoreClock / kPwmHz - 1;  // 799 at 16 MHz
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
        return false;
    }

    TIM_OC_InitTypeDef oc = {};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;  // high = MOSFET on = backlight on
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1) != HAL_OK) {
        return false;
    }

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
