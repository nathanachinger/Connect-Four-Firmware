#include "led_matrix.hpp"

#include "peripherals.hpp"

namespace led {
namespace {

constexpr uint16_t kI2cAddr = 0x74 << 1;  // AD pin tied to GND
constexpr uint32_t kTimeoutMs = 20;

constexpr uint8_t kRegCommand = 0xFD;  // selects the active register page
constexpr uint8_t kPageFrame0 = 0x00;
constexpr uint8_t kPageFunction = 0x0B;

// Function page
constexpr uint8_t kRegConfig = 0x00;        // 0x00 = picture mode
constexpr uint8_t kRegPictureFrame = 0x01;  // frame shown in picture mode
constexpr uint8_t kRegAudioSync = 0x06;
constexpr uint8_t kRegShutdown = 0x0A;      // 0 = software shutdown, 1 = normal

// Frame page
constexpr uint8_t kRegLedControl = 0x00;  // 18 bytes, on/off bit per LED
constexpr uint8_t kRegBlink = 0x12;       // 18 bytes
constexpr uint8_t kRegPwm = 0x24;         // 144 bytes, one per LED

constexpr uint8_t kRegisterBytes = 18;

constexpr uint8_t pwm_index(uint8_t row, uint8_t col, Color color) {
    return static_cast<uint8_t>(row * 16 + col + (color == Color::Yellow ? 8 : 0));
}

}  // namespace

bool Matrix::write_regs(uint8_t reg, const uint8_t* data, uint16_t len) {
    return HAL_I2C_Mem_Write(&hi2c1, kI2cAddr, reg, I2C_MEMADD_SIZE_8BIT, const_cast<uint8_t*>(data),
                             len, kTimeoutMs) == HAL_OK;
}

bool Matrix::write_reg(uint8_t reg, uint8_t value) { return write_regs(reg, &value, 1); }

bool Matrix::select_page(uint8_t page) { return write_reg(kRegCommand, page); }

bool Matrix::init() {
    ok_ = false;
    if (HAL_I2C_IsDeviceReady(&hi2c1, kI2cAddr, 3, kTimeoutMs) != HAL_OK) {
        return false;
    }

    // Configure while in software shutdown.
    if (!select_page(kPageFunction) || !write_reg(kRegShutdown, 0x00) ||
        !write_reg(kRegConfig, 0x00) || !write_reg(kRegPictureFrame, kPageFrame0) ||
        !write_reg(kRegAudioSync, 0x00)) {
        return false;
    }

    // Each row has two control bytes: matrix A (red, bits 0-6) then matrix B (yellow, bits 0-6).
    uint8_t led_control[kRegisterBytes] = {};
    for (uint8_t row = 0; row < kRows; ++row) {
        led_control[row * 2] = 0x7F;
        led_control[row * 2 + 1] = 0x7F;
    }
    const uint8_t no_blink[kRegisterBytes] = {};
    clear();

    if (!select_page(kPageFrame0) || !write_regs(kRegLedControl, led_control, kRegisterBytes) ||
        !write_regs(kRegBlink, no_blink, kRegisterBytes) || !write_regs(kRegPwm, pwm_, kPwmCount)) {
        return false;
    }

    // Leave shutdown, then park on frame 0 so show() only has to write PWM data.
    if (!select_page(kPageFunction) || !write_reg(kRegShutdown, 0x01) ||
        !select_page(kPageFrame0)) {
        return false;
    }

    ok_ = true;
    return true;
}

void Matrix::clear() {
    for (uint8_t& p : pwm_) {
        p = 0;
    }
}

void Matrix::fill(Color color, uint8_t brightness) {
    for (uint8_t row = 0; row < kRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            set(row, col, color, brightness);
        }
    }
}

void Matrix::set(uint8_t row, uint8_t col, Color color, uint8_t brightness) {
    if (row < kRows && col < kCols) {
        pwm_[pwm_index(row, col, color)] = brightness;
    }
}

bool Matrix::show() { return ok_ && write_regs(kRegPwm, pwm_, kPwmCount); }

}  // namespace led
