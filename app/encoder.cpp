#include "encoder.hpp"

#include "peripherals.hpp"

namespace encoder {
namespace {

constexpr uint32_t kDebounceMs = 20;

uint16_t last_raw = 0;
int32_t total = 0;

bool stable_down = false;
bool last_sample = false;
uint32_t last_change_ms = 0;
uint32_t presses = 0;

bool read_switch() {
    return HAL_GPIO_ReadPin(ENCODER_SEL_GPIO_Port, ENCODER_SEL_Pin) == GPIO_PIN_RESET;
}

}  // namespace

bool init() {
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = ENCODER_SEL_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;  // R1 is the pull-up
    HAL_GPIO_Init(ENCODER_SEL_GPIO_Port, &gpio);

    __HAL_TIM_SET_COUNTER(&htim3, 0);
    last_raw = 0;
    total = 0;
    stable_down = last_sample = read_switch();
    last_change_ms = HAL_GetTick();
    presses = 0;
    return HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL) == HAL_OK;
}

int32_t count() {
    const auto raw = static_cast<uint16_t>(__HAL_TIM_GET_COUNTER(&htim3));
    total += static_cast<int16_t>(static_cast<uint16_t>(raw - last_raw));
    last_raw = raw;
    return total;
}

void update_button() {
    const bool sample = read_switch();
    const uint32_t now = HAL_GetTick();
    if (sample != last_sample) {
        last_sample = sample;
        last_change_ms = now;
    } else if (sample != stable_down && now - last_change_ms >= kDebounceMs) {
        stable_down = sample;
        if (stable_down) {
            ++presses;
        }
    }
}

bool button_down() { return stable_down; }

uint32_t button_presses() { return presses; }

}  // namespace encoder
