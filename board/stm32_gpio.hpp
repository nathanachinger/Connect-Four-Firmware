#pragma once

#include <cstdint>

#include "gpio.hpp"
#include "main.h"

namespace board {

class Stm32GpioOut final : public hal::GpioOut {
public:
    Stm32GpioOut(GPIO_TypeDef* port, uint16_t pin, bool active_low = false);

    void set(bool on) override;
    void toggle() override;

private:
    GPIO_TypeDef* port_;
    uint16_t pin_;
    bool active_low_;
};

class Stm32GpioIn final : public hal::GpioIn {
public:
    Stm32GpioIn(GPIO_TypeDef* port, uint16_t pin, bool active_low = false);

    bool read() const override;

private:
    GPIO_TypeDef* port_;
    uint16_t pin_;
    bool active_low_;
};

}  // namespace board
