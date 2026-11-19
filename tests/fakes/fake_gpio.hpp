#pragma once

#include "gpio.hpp"

namespace fakes {

class FakeGpioOut final : public hal::GpioOut {
public:
    void set(bool on) override {
        if (on != state_) ++transitions_;
        state_ = on;
    }
    void toggle() override { set(!state_); }

    bool state() const { return state_; }
    int transitions() const { return transitions_; }

private:
    bool state_ = false;
    int transitions_ = 0;
};

class FakeGpioIn final : public hal::GpioIn {
public:
    bool read() const override { return state_; }
    void set(bool on) { state_ = on; }

private:
    bool state_ = false;
};

}  // namespace fakes
