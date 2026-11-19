#pragma once

#include <cstdint>

#include "clock.hpp"
#include "gpio.hpp"

namespace app {

// Top-level application. Owns no hardware; everything arrives through hal interfaces.
class App {
public:
    static constexpr uint32_t kHeartbeatPeriodMs = 500;

    App(hal::GpioOut& status_led, const hal::Clock& clock);

    // Call repeatedly from the main loop. Never blocks.
    void tick();

private:
    hal::GpioOut& status_led_;
    const hal::Clock& clock_;
    uint32_t last_toggle_ms_;
};

}  // namespace app
