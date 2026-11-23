#pragma once

#include <cstdint>

namespace input {

// Turns raw encoder counts into whole detent steps, carrying any partial step to the next call
// so a half-turned knob never skips or repeats.
class Knob {
public:
    void reset(int32_t count) { last_ = count; }
    // Detents moved since the last call: positive = clockwise.
    int32_t steps(int32_t count);

private:
    int32_t last_ = 0;
};

enum class Gesture : uint8_t { None, Click, Hold };

// Push-button gestures from raw samples. Pure logic: feed it (pressed, now_ms) every few ms.
//   Click: pressed and released within kHoldMs.
//   Hold:  fires once, while still held, after kHoldMs; the release that follows is ignored.
class Button {
public:
    static constexpr uint32_t kDebounceMs = 20;
    static constexpr uint32_t kHoldMs = 1500;

    void reset(bool pressed, uint32_t now_ms);
    Gesture update(bool pressed, uint32_t now_ms);
    bool down() const { return stable_; }

private:
    bool stable_ = false;
    bool last_sample_ = false;
    bool hold_fired_ = false;
    uint32_t last_change_ms_ = 0;
    uint32_t pressed_ms_ = 0;
};

struct Events {
    int32_t steps;  // detents since the last poll, positive = clockwise
    Gesture gesture;
    bool button_down;
};

// Reads the encoder hardware. Call init() once, then poll() every few ms.
bool init();
Events poll();

}  // namespace input
