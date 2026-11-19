#pragma once

#include <cstdint>

namespace hal {

// Monotonic millisecond time base. Wraps after ~49 days; compare with unsigned subtraction.
class Clock {
public:
    virtual uint32_t now_ms() const = 0;

protected:
    ~Clock() = default;
};

}  // namespace hal
