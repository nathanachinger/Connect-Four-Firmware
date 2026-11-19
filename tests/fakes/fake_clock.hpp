#pragma once

#include <cstdint>

#include "clock.hpp"

namespace fakes {

class FakeClock final : public hal::Clock {
public:
    uint32_t now_ms() const override { return now_; }

    void set(uint32_t ms) { now_ = ms; }
    void advance(uint32_t ms) { now_ += ms; }

private:
    uint32_t now_ = 0;
};

}  // namespace fakes
