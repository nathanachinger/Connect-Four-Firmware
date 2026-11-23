#include "display.hpp"

#include "font5x7.hpp"
#include "peripherals.hpp"

namespace lcd {
namespace {

constexpr uint32_t kTimeoutMs = 10;

// ST7565R commands
constexpr uint8_t kCmdDisplayOff = 0xAE;
constexpr uint8_t kCmdDisplayOn = 0xAF;
constexpr uint8_t kCmdAdcNormal = 0xA0;       // column 0 = SEG0
constexpr uint8_t kCmdComReverse = 0xC8;      // flip vertically (matches NHD sample code orientation)
constexpr uint8_t kCmdBias1_9 = 0xA2;
constexpr uint8_t kCmdPowerAll = 0x2F;        // booster + regulator + follower
constexpr uint8_t kCmdResistorRatio = 0x21;
constexpr uint8_t kCmdVolumeMode = 0x81;      // next byte: contrast 0..63
constexpr uint8_t kCmdStartLine0 = 0x40;
constexpr uint8_t kCmdAllPixelsOff = 0xA4;    // normal: show RAM
constexpr uint8_t kCmdAllPixelsOn = 0xA5;
constexpr uint8_t kCmdNormal = 0xA6;
constexpr uint8_t kCmdInverse = 0xA7;
constexpr uint8_t kCmdPage = 0xB0;
constexpr uint8_t kCmdColumnHigh = 0x10;
constexpr uint8_t kCmdColumnLow = 0x00;

constexpr uint8_t kDefaultContrast = 0x1F;

void cs(bool active) {
    HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void a0(bool is_data) {
    HAL_GPIO_WritePin(DISPLAY_A0_GPIO_Port, DISPLAY_A0_Pin, is_data ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool spi_write(const uint8_t* bytes, uint16_t len) {
    cs(true);
    const bool ok =
        HAL_SPI_Transmit(&hspi2, const_cast<uint8_t*>(bytes), len, kTimeoutMs) == HAL_OK;
    cs(false);
    return ok;
}

}  // namespace

bool Display::command(uint8_t cmd) { return commands(&cmd, 1); }

bool Display::commands(const uint8_t* cmds, uint16_t len) {
    a0(false);
    return spi_write(cmds, len);
}

bool Display::data(const uint8_t* bytes, uint16_t len) {
    a0(true);
    return spi_write(bytes, len);
}

bool Display::init() {
    ok_ = false;

    // SPI2 (mode 0, 8-bit, 2 MHz) and CS idle-high come from CubeMX. Don't raise the SPI clock
    // without also raising the PB10/PB11 GPIO speed: "Low" is rated for about 2 MHz.
    HAL_GPIO_WritePin(DISPLAY_nRST_GPIO_Port, DISPLAY_nRST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(DISPLAY_nRST_GPIO_Port, DISPLAY_nRST_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    const uint8_t init_seq[] = {
        kCmdAdcNormal, kCmdDisplayOff, kCmdComReverse, kCmdBias1_9, kCmdPowerAll,
        kCmdResistorRatio, kCmdVolumeMode, kDefaultContrast, kCmdStartLine0,
    };
    if (!commands(init_seq, sizeof init_seq)) {
        return false;
    }

    ok_ = true;
    clear();
    if (!show() || !command(kCmdDisplayOn)) {
        ok_ = false;
        return false;
    }
    return true;
}

void Display::set_contrast(uint8_t level) {
    const uint8_t cmds[] = {kCmdVolumeMode, static_cast<uint8_t>(level & 0x3F)};
    commands(cmds, sizeof cmds);
}

void Display::set_all_pixels_on(bool on) { command(on ? kCmdAllPixelsOn : kCmdAllPixelsOff); }

void Display::set_inverted(bool on) { command(on ? kCmdInverse : kCmdNormal); }

void Display::clear() {
    for (auto& page : buffer_) {
        for (uint8_t& column : page) {
            column = 0;
        }
    }
}

void Display::set_pixel(uint8_t x, uint8_t y, bool on) {
    if (x >= kWidth || y >= kHeight) {
        return;
    }
    const uint8_t bit = static_cast<uint8_t>(1u << (y & 7u));
    uint8_t& column = buffer_[y >> 3][x];
    column = on ? static_cast<uint8_t>(column | bit) : static_cast<uint8_t>(column & ~bit);
}

void Display::fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on) {
    for (uint8_t dy = 0; dy < h; ++dy) {
        for (uint8_t dx = 0; dx < w; ++dx) {
            set_pixel(static_cast<uint8_t>(x + dx), static_cast<uint8_t>(y + dy), on);
        }
    }
}

void Display::draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    if (w == 0 || h == 0) {
        return;
    }
    const uint8_t right = static_cast<uint8_t>(x + w - 1);
    const uint8_t bottom = static_cast<uint8_t>(y + h - 1);
    for (uint8_t i = 0; i < w; ++i) {
        set_pixel(static_cast<uint8_t>(x + i), y, true);
        set_pixel(static_cast<uint8_t>(x + i), bottom, true);
    }
    for (uint8_t i = 0; i < h; ++i) {
        set_pixel(x, static_cast<uint8_t>(y + i), true);
        set_pixel(right, static_cast<uint8_t>(y + i), true);
    }
}

uint8_t Display::text(uint8_t x, uint8_t line, const char* str) {
    if (line >= kPages) {
        return x;
    }
    for (; *str != '\0' && x + font::kWidth <= kWidth; ++str) {
        const uint8_t* glyph = font::glyph(*str);
        for (uint8_t i = 0; i < font::kWidth; ++i) {
            buffer_[line][x + i] = glyph[i];
        }
        buffer_[line][x + font::kWidth] = 0;  // 1 px gap
        x = static_cast<uint8_t>(x + kCharWidth);
    }
    return x;
}

void Display::number(uint8_t end_x, uint8_t line, int32_t value) {
    char digits[12];
    char* p = digits + sizeof digits - 1;
    *p = '\0';
    const bool negative = value < 0;
    uint32_t magnitude = negative ? 0u - static_cast<uint32_t>(value) : static_cast<uint32_t>(value);
    do {
        *--p = static_cast<char>('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    if (negative) {
        *--p = '-';
    }
    const auto len = static_cast<uint8_t>(digits + sizeof digits - 1 - p);
    const uint8_t width = static_cast<uint8_t>(len * kCharWidth);
    text(end_x >= width ? static_cast<uint8_t>(end_x - width) : 0, line, p);
}

void Display::invert_line(uint8_t line, uint8_t x, uint8_t w) {
    if (line >= kPages) {
        return;
    }
    for (uint16_t i = x; i < kWidth && i < static_cast<uint16_t>(x + w); ++i) {
        buffer_[line][i] = static_cast<uint8_t>(~buffer_[line][i]);
    }
}

bool Display::show() {
    if (!ok_) {
        return false;
    }
    for (uint8_t page = 0; page < kPages; ++page) {
        const uint8_t addr[] = {static_cast<uint8_t>(kCmdPage | page), kCmdColumnHigh,
                                kCmdColumnLow};
        if (!commands(addr, sizeof addr) || !data(buffer_[page], kWidth)) {
            return false;
        }
    }
    return true;
}

}  // namespace lcd
