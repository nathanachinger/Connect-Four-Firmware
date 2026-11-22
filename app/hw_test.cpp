#include "hw_test.hpp"

#include <initializer_list>

#include "audio.hpp"
#include "backlight.hpp"
#include "adc.hpp"
#include "battery.hpp"
#include "encoder.hpp"
#include "light_sensor.hpp"
#include "peripherals.hpp"

namespace hw_test {

volatile Results g_results = {};

namespace {

using led::Color;
using led::kCols;
using led::kRows;

constexpr uint8_t kBrightness = 128;
constexpr uint16_t kVolume = 64;  // 25% of full DAC swing

void show_for(led::Matrix& matrix, uint32_t ms) {
    matrix.show();
    HAL_Delay(ms);
}

void set_both(led::Matrix& matrix, uint8_t row, uint8_t col, uint8_t brightness) {
    matrix.set(row, col, Color::Red, brightness);
    matrix.set(row, col, Color::Yellow, brightness);
}

// Rapid LOW_BAT_LED flashes mark a failed test.
void flash_error(uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        HAL_GPIO_WritePin(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin, GPIO_PIN_SET);
        HAL_Delay(80);
        HAL_GPIO_WritePin(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin, GPIO_PIN_RESET);
        HAL_Delay(80);
    }
    HAL_Delay(500);
}

// Formats millivolts as "6.42V" into buf (at least 8 bytes).
const char* format_volts(char* buf, uint32_t mv) {
    const uint32_t centivolts = (mv + 5) / 10;
    const uint32_t whole = centivolts / 100;
    const uint32_t frac = centivolts % 100;
    char* p = buf;
    if (whole >= 10) {
        *p++ = static_cast<char>('0' + whole / 10 % 10);
    }
    *p++ = static_cast<char>('0' + whole % 10);
    *p++ = '.';
    *p++ = static_cast<char>('0' + frac / 10);
    *p++ = static_cast<char>('0' + frac % 10);
    *p++ = 'V';
    *p = '\0';
    return buf;
}

}  // namespace

void display(lcd::Display& screen) {
    g_results.backlight_ok = backlight::init();
    backlight::set_percent(100);
    g_results.display_ok = screen.init();
    if (!g_results.display_ok) {
        flash_error(4);
        return;
    }

    // Every pixel on via the controller's test mode (doesn't use display RAM).
    screen.set_all_pixels_on(true);
    HAL_Delay(1000);
    screen.set_all_pixels_on(false);

    // Orientation and font: the border should touch all four edges and text should read
    // left to right, top line first.
    screen.clear();
    screen.draw_rect(0, 0, lcd::kWidth, lcd::kHeight);
    screen.text(28, 0, "CONNECT FOUR");
    screen.text(31, 1, "SCREEN TEST");
    screen.text(2, 2, "ABCDEFGHIJKLMNOPQRST");
    screen.text(2, 3, "0123456789 UVWXYZ!?");
    screen.show();
    HAL_Delay(2500);

    screen.set_inverted(true);
    HAL_Delay(1000);
    screen.set_inverted(false);

    // 4x4 checkerboard: dead rows/columns or stuck pixels stand out.
    screen.clear();
    for (uint8_t y = 0; y < lcd::kHeight; ++y) {
        for (uint8_t x = 0; x < lcd::kWidth; ++x) {
            screen.set_pixel(x, y, ((x / 4) + (y / 4)) % 2 == 0);
        }
    }
    screen.show();
    HAL_Delay(1500);

    // Contrast sweep 0..63. Note the value where text looks best.
    for (uint8_t level = 0; level < 64; level = static_cast<uint8_t>(level + 3)) {
        screen.clear();
        screen.text(0, 0, "CONTRAST SWEEP");
        screen.text(0, 2, "LEVEL");
        screen.number(60, 2, level);
        screen.fill_rect(0, 26, static_cast<uint8_t>(level * 2), 6, true);
        screen.show();
        screen.set_contrast(level);
        HAL_Delay(150);
    }
    screen.set_contrast(0x1F);

    // Backlight PWM: off -> full -> off. Should fade smoothly with no flicker or whine.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i <= 100; i += 2) {
            const auto pct = static_cast<uint8_t>(pass == 0 ? i : 100 - i);
            backlight::set_percent(pct);
            screen.clear();
            screen.text(0, 0, "BACKLIGHT SWEEP");
            screen.number(42, 2, pct);
            screen.text(42, 2, "%");
            screen.fill_rect(0, 26, static_cast<uint8_t>(pct * 127 / 100), 6, true);
            screen.show();
            HAL_Delay(30);
        }
    }
    backlight::set_percent(100);
    screen.clear();
    screen.show();
}

void led_matrix(led::Matrix& matrix) {
    g_results.led_ok = matrix.init();
    if (!g_results.led_ok) {
        flash_error(5);
        return;
    }

    // Whole grids: every red LED, every yellow LED, then both (orange).
    matrix.clear();
    matrix.fill(Color::Red, kBrightness);
    show_for(matrix, 1000);
    matrix.clear();
    matrix.fill(Color::Yellow, kBrightness);
    show_for(matrix, 1000);
    matrix.fill(Color::Red, kBrightness);
    show_for(matrix, 1000);

    // Row sweep starting at row 0 (C1-x), then column sweep starting at column 0.
    for (uint8_t row = 0; row < kRows; ++row) {
        matrix.clear();
        for (uint8_t col = 0; col < kCols; ++col) {
            set_both(matrix, row, col, kBrightness);
        }
        show_for(matrix, 250);
    }
    for (uint8_t col = 0; col < kCols; ++col) {
        matrix.clear();
        for (uint8_t row = 0; row < kRows; ++row) {
            set_both(matrix, row, col, kBrightness);
        }
        show_for(matrix, 250);
    }

    // One LED at a time, row by row: all red, then all yellow. A gap or out-of-order LED
    // points at a wiring or mapping problem.
    for (Color color : {Color::Red, Color::Yellow}) {
        for (uint8_t row = 0; row < kRows; ++row) {
            for (uint8_t col = 0; col < kCols; ++col) {
                matrix.clear();
                matrix.set(row, col, color, kBrightness);
                show_for(matrix, 60);
            }
        }
    }

    // PWM ramp up and down on everything.
    for (int b = 0; b <= 255; b += 5) {
        matrix.fill(Color::Red, static_cast<uint8_t>(b));
        matrix.fill(Color::Yellow, static_cast<uint8_t>(b));
        show_for(matrix, 15);
    }
    for (int b = 255; b >= 0; b -= 5) {
        matrix.fill(Color::Red, static_cast<uint8_t>(b));
        matrix.fill(Color::Yellow, static_cast<uint8_t>(b));
        show_for(matrix, 15);
    }

    matrix.clear();
    show_for(matrix, 300);
}

void audio(led::Matrix& matrix) {
    static constexpr uint32_t kScaleHz[] = {523, 587, 659, 698, 784, 880, 988, 1047};  // C5..C6

    audio::begin();

    // Scale: each note also lights one more column of the bottom row (if the matrix works).
    matrix.clear();
    for (uint8_t i = 0; i < sizeof(kScaleHz) / sizeof(kScaleHz[0]); ++i) {
        matrix.set(kRows - 1, i < kCols ? i : kCols - 1, Color::Red, kBrightness);
        matrix.show();
        audio::tone(kScaleHz[i], 200, kVolume);
        audio::silence(50);
    }
    audio::silence(300);

    // Sweep 200 Hz -> 4 kHz to check the speaker and filter across the audio range.
    audio::sweep(200, 4000, 2000, kVolume);
    audio::silence(300);

    // 1 kHz at 1/16, 1/8, 1/4 and 1/2 of full swing. Listen for distortion on the loudest.
    static constexpr uint16_t kVolumes[] = {16, 32, 64, 128};
    for (uint16_t volume : kVolumes) {
        audio::tone(1000, 400, volume);
        audio::silence(150);
    }

    audio::end();
    matrix.clear();
    matrix.show();
    g_results.audio_done = true;
}

void live(lcd::Display& screen, led::Matrix& matrix) {
    g_results.light_ok = light::init();
    g_results.encoder_ok = encoder::init();
    g_results.battery_ok = battery::init();
    g_results.light_min = 0xFFFF;
    g_results.light_max = 0;
    if (!g_results.light_ok) {
        flash_error(3);
    }
    if (!g_results.encoder_ok) {
        flash_error(2);
    }

    int32_t backlight_pct = 100;
    uint32_t last_draw_ms = HAL_GetTick();
    uint32_t last_toggle_ms = last_draw_ms;
    for (;;) {
        // Sample the button every ~5 ms so debouncing works; redraw at 20 Hz.
        encoder::update_button();
        HAL_Delay(5);
        const uint32_t now = HAL_GetTick();
        if (now - last_draw_ms < 50) {
            continue;
        }
        last_draw_ms = now;

        const int32_t count = encoder::count();
        const int32_t detents = count / encoder::kCountsPerDetent;
        const bool down = encoder::button_down();
        g_results.encoder_count = count;
        g_results.encoder_detents = detents;
        g_results.button_down = down;
        g_results.button_presses = encoder::button_presses();

        uint16_t raw = 0;
        if (g_results.light_ok) {
            raw = light::read_raw();
            g_results.light_raw = raw;
            g_results.light_mv = light::raw_to_mv(raw);
            if (raw < g_results.light_min) {
                g_results.light_min = raw;
            }
            if (raw > g_results.light_max) {
                g_results.light_max = raw;
            }
        }

        // Auto backlight: brighter room -> brighter backlight, 10% floor so it stays readable
        // in the dark. Smoothed so it doesn't jump with every reading.
        if (g_results.light_ok && g_results.backlight_ok) {
            const int32_t target = 10 + raw * 90 / 4095;
            backlight_pct += (target - backlight_pct) / 4;
            backlight::set_percent(static_cast<uint8_t>(backlight_pct));
        }
        g_results.backlight_pct = static_cast<uint8_t>(backlight_pct);

        if (g_results.battery_ok) {
            g_results.vdda_mv = adc::measure_vdda_mv();
            g_results.battery_mv = battery::read_mv();
        }

        // Screen: 4 lines of readings, cursor block on the bottom line (16 positions).
        screen.clear();
        screen.text(0, 0, "ENC");
        screen.number(60, 0, count);
        screen.text(72, 0, "DET");
        screen.number(126, 0, detents);
        screen.text(0, 1, "BTN");
        screen.text(24, 1, down ? "DN" : "UP");
        screen.number(60, 1, static_cast<int32_t>(g_results.button_presses));
        screen.text(72, 1, "BL");
        screen.number(120, 1, backlight_pct);
        screen.text(120, 1, "%");
        screen.text(0, 2, "LIGHT");
        screen.number(60, 2, raw);
        screen.text(72, 2, "BAT");
        char volts[8];
        screen.text(96, 2, g_results.battery_ok ? format_volts(volts, g_results.battery_mv) : "ERR");
        const auto cursor = static_cast<uint8_t>(((detents % 16) + 16) % 16);
        if (down) {
            screen.fill_rect(0, 24, lcd::kWidth, 8, true);
            screen.fill_rect(static_cast<uint8_t>(cursor * 8), 24, 8, 8, false);
        } else {
            screen.fill_rect(static_cast<uint8_t>(cursor * 8), 24, 8, 8, true);
        }
        screen.show();

        // Matrix: top row = encoder cursor (red, orange while pressed);
        // rows 1-6 = light bar rising from the bottom (0-6 rows).
        matrix.clear();
        const auto col = static_cast<uint8_t>(((detents % kCols) + kCols) % kCols);
        matrix.set(0, col, Color::Red, kBrightness);
        if (down) {
            matrix.set(0, col, Color::Yellow, kBrightness);
        }
        const auto rows_lit = static_cast<uint8_t>(raw * kRows / 4096u);  // 0..6
        for (uint8_t i = 0; i < rows_lit; ++i) {
            for (uint8_t c = 0; c < kCols; ++c) {
                matrix.set(static_cast<uint8_t>(kRows - 1 - i), c, Color::Yellow, kBrightness);
            }
        }
        matrix.show();

        // 1 Hz heartbeat so you can tell the loop is running.
        if (now - last_toggle_ms >= 500) {
            last_toggle_ms += 500;
            HAL_GPIO_TogglePin(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin);
        }
    }
}

void all_red(led::Matrix& matrix) {
    g_results.led_ok = matrix.init();
    for (;;) {
        if (!g_results.led_ok) {
            flash_error(5);
            g_results.led_ok = matrix.init();
            continue;
        }
        matrix.clear();
        matrix.fill(Color::Red, kBrightness);
        matrix.show();
        HAL_Delay(500);
    }
}

void run_all() {
    static lcd::Display screen;
    static led::Matrix matrix;

    display(screen);
    led_matrix(matrix);
    audio(matrix);
    live(screen, matrix);
}

}  // namespace hw_test
