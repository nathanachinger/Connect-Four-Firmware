#pragma once

namespace hal {

// Digital output. `set(true)` means "active" (implementations handle polarity).
class GpioOut {
public:
    virtual void set(bool on) = 0;
    virtual void toggle() = 0;

protected:
    ~GpioOut() = default;
};

// Digital input. `read()` returns true when "active" (implementations handle polarity).
class GpioIn {
public:
    virtual bool read() const = 0;

protected:
    ~GpioIn() = default;
};

}  // namespace hal
