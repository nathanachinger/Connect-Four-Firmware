#pragma once

#include "clock.hpp"

namespace board {

// 1 ms time base from the HAL SysTick counter.
class Stm32Clock final : public hal::Clock {
public:
    uint32_t now_ms() const override;
};

}  // namespace board
