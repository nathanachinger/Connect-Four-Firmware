#pragma once

#include <cstdint>

namespace lcd {

inline constexpr uint8_t kWidth = 128;
inline constexpr uint8_t kHeight = 32;
inline constexpr uint8_t kPages = kHeight / 8;

// Glyphs are 5x7 in a 6x8 cell, so 21 characters x 4 lines.
inline constexpr uint8_t kCharWidth = 6;

// NHD-C12832A1Z (ST7565R) on SPI2 (write-only), CS PB8, A0 PB9, nRST PB7.
// The backlight is separate: see backlight.hpp.
class Display {
public:
    // Resets the controller and turns the display on.
    bool init();

    // Electronic volume, 0..63. Higher = darker.
    void set_contrast(uint8_t level);
    // Hardware test modes that don't touch display RAM.
    void set_all_pixels_on(bool on);
    void set_inverted(bool on);

    // These edit the local frame buffer; call show() to send it.
    void clear();
    void set_pixel(uint8_t x, uint8_t y, bool on);
    void fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
    void draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    // Draws text on one of the 4 text lines (8 px each). Lowercase prints as uppercase;
    // unsupported characters print as spaces. Returns the x after the last character.
    uint8_t text(uint8_t x, uint8_t line, const char* str);
    // Right-aligns a signed number so it ends at column end_x.
    void number(uint8_t end_x, uint8_t line, int32_t value);
    // Flips every pixel of a text line (8 px tall) from x to x + w, e.g. for a selection bar.
    void invert_line(uint8_t line, uint8_t x = 0, uint8_t w = kWidth);

    bool show();

private:
    bool command(uint8_t cmd);
    bool commands(const uint8_t* cmds, uint16_t len);
    bool data(const uint8_t* bytes, uint16_t len);

    uint8_t buffer_[kPages][kWidth] = {};
    bool ok_ = false;
};

}  // namespace lcd
