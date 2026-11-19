#include "stm32_gpio.hpp"

namespace board {

Stm32GpioOut::Stm32GpioOut(GPIO_TypeDef* port, uint16_t pin, bool active_low)
    : port_(port), pin_(pin), active_low_(active_low) {}

void Stm32GpioOut::set(bool on) {
    HAL_GPIO_WritePin(port_, pin_, (on != active_low_) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Stm32GpioOut::toggle() { HAL_GPIO_TogglePin(port_, pin_); }

Stm32GpioIn::Stm32GpioIn(GPIO_TypeDef* port, uint16_t pin, bool active_low)
    : port_(port), pin_(pin), active_low_(active_low) {}

bool Stm32GpioIn::read() const {
    const bool high = HAL_GPIO_ReadPin(port_, pin_) == GPIO_PIN_SET;
    return high != active_low_;
}

}  // namespace board
