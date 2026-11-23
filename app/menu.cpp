#include "menu.hpp"

#include <cstring>

namespace menu {
namespace {

constexpr uint8_t kFirstItemLine = 0;
constexpr uint8_t kVisible = 3;  // lines 0-2; line 3 is the hint
constexpr uint8_t kLabelX = 2 * lcd::kCharWidth;

void centered(lcd::Display& screen, uint8_t line, const char* text) {
    const auto width = static_cast<uint32_t>(std::strlen(text)) * lcd::kCharWidth;
    const auto x = static_cast<uint8_t>(width < lcd::kWidth ? (lcd::kWidth - width) / 2 : 0);
    screen.text(x, line, text);
}

}  // namespace

void Menu::start() {
    render_screen();
    render_leds();
}

Choice Menu::update(const input::Events& in) {
    if (in.gesture == input::Gesture::Hold) {
        return Choice::Back;
    }
    if (in.steps != 0) {
        int32_t s = selected_ + in.steps;
        s = s < 0 ? 0 : (s >= count_ ? count_ - 1 : s);
        if (s != selected_) {
            selected_ = static_cast<uint8_t>(s);
            // Scroll just enough to keep the selection visible.
            if (selected_ < top_) {
                top_ = selected_;
            } else if (selected_ >= top_ + kVisible) {
                top_ = static_cast<uint8_t>(selected_ - kVisible + 1);
            }
            render_screen();
        }
    }

    if (in.gesture == input::Gesture::Click && items_[selected_].enabled) {
        return items_[selected_].choice;
    }
    return Choice::None;
}

void Menu::render_screen() {
    screen_.clear();
    for (uint8_t i = 0; i < kVisible && top_ + i < count_; ++i) {
        const uint8_t item = top_ + i;
        const auto line = static_cast<uint8_t>(kFirstItemLine + i);
        screen_.text(kLabelX, line, items_[item].label);
        if (item == selected_) {
            screen_.invert_line(line);
        }
    }
    centered(screen_, 3, hint_);
    screen_.show();
}

void Menu::render_leds() {
    // The board stays dark in menus.
    matrix_.clear();
    matrix_.show();
}

}  // namespace menu
