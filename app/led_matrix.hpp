#pragma once

#include <cstdint>

namespace led {

enum class Color : uint8_t { Red, Yellow };

inline constexpr uint8_t kRows = 7;
inline constexpr uint8_t kCols = 7;

// IS31FL3731 driving the red and yellow 7x7 grids over I2C1.
//
// row/col are physical, as seen on the board: row 0 = top, col 0 = left.
// The grid is transposed relative to the schematic's Cx-y names (confirmed on hardware):
//   row r, col c, red    -> LED C(c+1)-(r+1)   (matrix A, CA lines)
//   row r, col c, yellow -> LED C(c+1)-(r+9)   (matrix B, CB lines)
class Matrix {
public:
    // Probes the chip, configures picture mode on frame 0 and enables the 98 fitted LEDs.
    bool init();
    bool ok() const { return ok_; }

    // These edit the local frame buffer; call show() to send it.
    void clear();
    void fill(Color color, uint8_t brightness);
    void set(uint8_t row, uint8_t col, Color color, uint8_t brightness);

    bool show();

private:
    static constexpr uint8_t kPwmCount = 144;

    bool write_reg(uint8_t reg, uint8_t value);
    bool write_regs(uint8_t reg, const uint8_t* data, uint16_t len);
    bool select_page(uint8_t page);

    uint8_t pwm_[kPwmCount] = {};
    bool ok_ = false;
};

}  // namespace led
