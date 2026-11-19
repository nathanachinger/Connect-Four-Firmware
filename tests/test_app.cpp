#include <gtest/gtest.h>

#include "app.hpp"
#include "fake_clock.hpp"
#include "fake_gpio.hpp"

namespace {

using app::App;

TEST(AppHeartbeat, DoesNotToggleBeforePeriod) {
    fakes::FakeClock clock;
    fakes::FakeGpioOut led;
    App a(led, clock);

    clock.advance(App::kHeartbeatPeriodMs - 1);
    a.tick();
    EXPECT_EQ(led.transitions(), 0);
}

TEST(AppHeartbeat, TogglesEachPeriod) {
    fakes::FakeClock clock;
    fakes::FakeGpioOut led;
    App a(led, clock);

    for (int i = 1; i <= 4; ++i) {
        clock.advance(App::kHeartbeatPeriodMs);
        a.tick();
        EXPECT_EQ(led.transitions(), i);
    }
}

TEST(AppHeartbeat, SurvivesTickCounterWrap) {
    fakes::FakeClock clock;
    clock.set(0xFFFFFFFFu - 100u);
    fakes::FakeGpioOut led;
    App a(led, clock);

    clock.advance(App::kHeartbeatPeriodMs);  // wraps past zero
    a.tick();
    EXPECT_EQ(led.transitions(), 1);
}

}  // namespace
