#pragma once

#include <cstdint>

#include "display.hpp"
#include "led_matrix.hpp"

// Bring-up tests for the screen, LED matrix, audio, photoresistor and rotary encoder.
// There is no serial port, so results go to g_results (add it to a debugger watch window),
// to the screen and LED matrix, and to LOW_BAT_LED.
namespace hw_test {

struct Results {
    bool backlight_ok;   // TIM1 PWM started on PA8
    uint8_t backlight_pct;  // current backlight setting, 0..100
    bool display_ok;     // SPI2 configured and init commands sent (the LCD can't be read back)
    bool led_ok;         // IS31FL3731 acked on I2C and accepted its configuration
    bool audio_done;     // audio sequence finished
    bool light_ok;       // ADC configured for the photoresistor
    bool encoder_ok;     // TIM3 encoder mode started
    uint16_t light_raw;  // latest averaged reading, 0..4095 (brighter = higher)
    uint32_t light_mv;
    uint16_t light_min;  // lowest/highest readings since the live test started
    uint16_t light_max;
    bool battery_ok;      // ADC configured for the battery divider
    uint32_t battery_mv;  // battery voltage at +VBAT_SW (after the power switch), averaged
    uint32_t vdda_mv;     // measured ADC reference (3V3 rail) used for the conversions
    int32_t encoder_count;  // raw x4 counts since start (clockwise should be positive)
    int32_t encoder_detents;
    bool button_down;
    uint32_t button_presses;
};

extern volatile Results g_results;

// ~13 s: all pixels on, text/orientation page, inverse, checkerboard, contrast sweep,
// backlight PWM sweep.
void display(lcd::Display& screen);

// ~15 s: full red, full yellow, both, row sweep, column sweep, single-LED walk, brightness ramp.
void led_matrix(led::Matrix& matrix);

// ~6 s: C major scale, frequency sweep, then the same tone at rising volumes.
void audio(led::Matrix& matrix);

// Runs forever: encoder count, button, light level, backlight and battery voltage on the
// screen, with the backlight following room light;
// an encoder cursor on the matrix top row and a light-level bar below it.
[[noreturn]] void live(lcd::Display& screen, led::Matrix& matrix);

// Runs forever: every red LED on at test brightness (wiring debug).
[[noreturn]] void all_red(led::Matrix& matrix);

// Screen, LED matrix and audio tests, then the live loop.
[[noreturn]] void run_all();

}  // namespace hw_test
