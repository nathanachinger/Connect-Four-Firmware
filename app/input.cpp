#include "input.hpp"

#include "encoder.hpp"
#include "peripherals.hpp"

namespace input {

int32_t Knob::steps(int32_t count) {
    // Truncating division keeps the remainder (either sign) in last_ for next time.
    const int32_t n = (count - last_) / encoder::kCountsPerDetent;
    last_ += n * encoder::kCountsPerDetent;
    return n;
}

void Button::reset(bool pressed, uint32_t now_ms) {
    stable_ = last_sample_ = pressed;
    // A button held at reset (e.g. through a screen change) must be released before it counts.
    hold_fired_ = pressed;
    last_change_ms_ = pressed_ms_ = now_ms;
}

Gesture Button::update(bool pressed, uint32_t now_ms) {
    if (pressed != last_sample_) {
        last_sample_ = pressed;
        last_change_ms_ = now_ms;
        return Gesture::None;
    }

    if (pressed != stable_ && now_ms - last_change_ms_ >= kDebounceMs) {
        stable_ = pressed;
        if (stable_) {
            pressed_ms_ = now_ms;
            hold_fired_ = false;
            return Gesture::None;
        }
        return hold_fired_ ? Gesture::None : Gesture::Click;
    }

    if (stable_ && !hold_fired_ && now_ms - pressed_ms_ >= kHoldMs) {
        hold_fired_ = true;
        return Gesture::Hold;
    }
    return Gesture::None;
}

namespace {
Knob g_knob;
Button g_button;
}  // namespace

bool init() {
    const bool ok = encoder::init();
    g_knob.reset(encoder::count());
    g_button.reset(encoder::switch_pressed(), HAL_GetTick());
    return ok;
}

Events poll() {
    Events e;
    e.steps = g_knob.steps(encoder::count());
    e.gesture = g_button.update(encoder::switch_pressed(), HAL_GetTick());
    e.button_down = g_button.down();
    return e;
}

}  // namespace input
