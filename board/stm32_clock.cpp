#include "stm32_clock.hpp"

#include "main.h"

namespace board {

uint32_t Stm32Clock::now_ms() const { return HAL_GetTick(); }

}  // namespace board
