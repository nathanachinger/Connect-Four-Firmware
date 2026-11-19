#include "app.hpp"

namespace app {

App::App(hal::GpioOut& status_led, const hal::Clock& clock)
    : status_led_(status_led), clock_(clock), last_toggle_ms_(clock.now_ms()) {}

void App::tick() {
    // M0 bring-up heartbeat. Replaced by the real state machine in M7.
    const uint32_t now = clock_.now_ms();
    if (now - last_toggle_ms_ >= kHeartbeatPeriodMs) {
        last_toggle_ms_ += kHeartbeatPeriodMs;
        status_led_.toggle();
    }
}

}  // namespace app
