#include "settings_screen.hpp"

#include <cstring>

#include "settings.hpp"
#include "sound.hpp"

namespace settings_screen {
namespace {

constexpr uint8_t kItemCount = 3;
enum Item : uint8_t { kBrightness, kSoundEffects, kDebug };
constexpr uint8_t kLabelX = 2 * lcd::kCharWidth;
constexpr uint8_t kValueEndX = lcd::kWidth - lcd::kCharWidth;  // right edge of the value text


void centered(lcd::Display& screen, uint8_t line, const char* text) {
    const auto width = static_cast<uint32_t>(std::strlen(text)) * lcd::kCharWidth;
    const auto x = static_cast<uint8_t>(width < lcd::kWidth ? (lcd::kWidth - width) / 2 : 0);
    screen.text(x, line, text);
}

// "AUTO" or "1".."10" into buf.
void format_level(char* buf, uint8_t level, const char* zero_name) {
    if (level == 0) {
        std::strcpy(buf, zero_name);
        return;
    }
    uint8_t n = 0;
    if (level >= 10) {
        buf[n++] = static_cast<char>('0' + level / 10);
    }
    buf[n++] = static_cast<char>('0' + level % 10);
    buf[n] = '\0';
}

}  // namespace

void Screen::start() {
    editing_ = false;
    render_screen();
    render_leds();
}

bool Screen::update(const input::Events& in, uint32_t now_ms) {
    if (in.gesture == input::Gesture::Hold) {
        return false;
    }

    if (in.gesture == input::Gesture::Click) {
        settings::Values& v = settings::values();
        // On/off items need no edit step: a click switches them.
        if (selected_ == kSoundEffects) {
            v.sound_effects = !v.sound_effects;
            if (v.sound_effects) {
                sound::play(sound::Id::SoundOn, now_ms);
            } else {
                sound::stop();  // cut anything still playing; silence confirms it's off
            }
        } else if (selected_ == kDebug) {
            v.debug = !v.debug;
            sound::play(sound::Id::MenuSelect, now_ms);
        } else {
            editing_ = !editing_;
            sound::play(sound::Id::MenuSelect, now_ms);
        }
        render_screen();
    } else if (in.steps != 0) {
        if (editing_) {
            adjust(in.steps, now_ms);
        } else {
            int32_t s = selected_ + in.steps;
            s = s < 0 ? 0 : (s >= kItemCount ? kItemCount - 1 : s);
            if (s != selected_) {
                selected_ = static_cast<uint8_t>(s);
                render_screen();
                sound::play(sound::Id::MenuMove, now_ms);
            }
        }
    }

    // Auto brightness can change the LED level on its own.
    if (settings::led_pwm() != shown_pwm_) {
        render_leds();
    }
    return true;
}

// Only brightness has an edit mode.
void Screen::adjust(int32_t steps, uint32_t now_ms) {
    settings::Values& v = settings::values();
    // Brightness runs AUTO, 1, 2 ... 10. Leaving AUTO starts from the level it was showing, so
    // the brightness doesn't jump.
    int32_t level = v.auto_brightness ? 0 : v.brightness;
    if (v.auto_brightness && steps > 0) {
        level = settings::nearest_level() + (steps - 1);
    } else {
        level += steps;
    }
    level = level < 0 ? 0 : (level > settings::kMaxLevel ? settings::kMaxLevel : level);
    const int32_t before = v.auto_brightness ? 0 : v.brightness;
    if (level == before) {
        return;  // already at the end of the range
    }
    v.auto_brightness = level == 0;
    if (level != 0) {
        v.brightness = static_cast<uint8_t>(level);
    }
    settings::update(now_ms);  // apply right away
    sound::play(sound::Id::MenuMove, now_ms);
    render_screen();
}

void Screen::render_screen() {
    const settings::Values& v = settings::values();
    screen_.clear();

    for (uint8_t i = 0; i < kItemCount; ++i) {
        char value[12];
        const char* label = "";
        switch (i) {
            case kBrightness:
                label = "BRIGHTNESS";
                format_level(value, v.auto_brightness ? 0 : v.brightness, "AUTO");
                break;
            case kSoundEffects:
                label = "SOUND EFFECTS";
                std::strcpy(value, v.sound_effects ? "ON" : "OFF");
                break;
            default:
                label = "DEBUG";
                std::strcpy(value, v.debug ? "ON" : "OFF");
                break;
        }
        const bool editing_this = editing_ && i == selected_;
        if (editing_this) {
            char framed[16] = "<";
            std::strcat(framed, value);
            std::strcat(framed, ">");
            std::strcpy(value, framed);
        }
        const auto value_w = static_cast<uint8_t>(std::strlen(value) * lcd::kCharWidth);
        const auto value_x = static_cast<uint8_t>(kValueEndX - value_w);

        screen_.text(kLabelX, i, label);
        screen_.text(value_x, i, value);
        if (editing_this) {
            // Highlight just the value being changed.
            screen_.invert_line(i, static_cast<uint8_t>(value_x - 2),
                                static_cast<uint8_t>(value_w + 3));
        } else if (i == selected_) {
            screen_.invert_line(i);
        }
    }

    const char* hint = editing_ ? "TURN=ADJUST CLICK=OK"
                       : selected_ != kBrightness ? "CLICK=ON/OFF"
                                        : "CLICK=EDIT HOLD=BACK";
    centered(screen_, 3, hint);
    screen_.show();
}

void Screen::render_leds() {
    // Sample of the board brightness: the bottom row in both colors.
    shown_pwm_ = settings::led_pwm();
    matrix_.clear();
    for (uint8_t col = 0; col < led::kCols; ++col) {
        matrix_.set(led::kRows - 1, col, col % 2 == 0 ? led::Color::Red : led::Color::Yellow,
                    shown_pwm_);
    }
    matrix_.show();
}

}  // namespace settings_screen
