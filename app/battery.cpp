#include "battery.hpp"

#include "adc.hpp"
#include "peripherals.hpp"

namespace battery {
namespace {

constexpr uint32_t kDividerTop = 120;     // kOhm, R13
constexpr uint32_t kDividerBottom = 100;  // kOhm, R15

}  // namespace

bool init() { return adc::init(); }

uint32_t read_mv() {
    const uint32_t pin_mv = adc::to_mv(adc::read(ADC_CHANNEL_0));
    return pin_mv * (kDividerTop + kDividerBottom) / kDividerBottom;
}

}  // namespace battery
