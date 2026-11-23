#pragma once

#include <cstdint>

namespace settings {

inline constexpr uint8_t kMaxLevel = 10;

// Brightness applies to both the LCD backlight and the LED board.
// Saved in flash (see flash_store.hpp) and restored at power-up.
struct Values {
    bool auto_brightness = true;  // follow the room light (photoresistor)
    uint8_t brightness = 7;       // manual level, 1..kMaxLevel
    bool sound_effects = true;
};

Values& values();

// Restores the saved values, if there are any valid ones; otherwise keeps the defaults.
void load();
// Saves the current values (no flash write if they haven't changed). Returns false on a flash
// error. The CPU stalls for up to ~25 ms while flash is written.
bool save();

// Reads the light sensor once so auto brightness starts at the right level instead of ramping.
void init();
// Call every loop: tracks room light in auto mode and drives the backlight.
void update(uint32_t now_ms);

// Manual level (1..kMaxLevel) closest to the brightness showing now.
uint8_t nearest_level();
// Current LED board brightness (IS31FL3731 PWM, 1..255) for renderers.
uint8_t led_pwm();
// Speaker gain for audio::play_*: a fixed level with sound effects on, 0 with them off.
uint16_t sound_gain();

// Pure mappings, exposed for testing.
// Brightness percent (10..100) -> LED PWM, squared so steps look even to the eye.
uint8_t led_pwm_for_percent(uint8_t percent);
// Ambient light reading (0..4095, brighter = higher) -> brightness percent (10..100).
uint8_t auto_percent_for_light(uint16_t raw);

}  // namespace settings
