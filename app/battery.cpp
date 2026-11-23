#include "battery.hpp"

#include "adc.hpp"
#include "peripherals.hpp"

namespace battery {
namespace {

constexpr uint32_t kDividerTop = 120;     // kOhm, R13
constexpr uint32_t kDividerBottom = 100;  // kOhm, R15

// Per-cell voltage vs. charge left for an alkaline AA at light load. The curve is steep near
// full and empty and flatter in between, so a straight line from 1.0 V to 1.6 V would read
// high for most of the battery's life.
struct Point {
    uint16_t cell_mv;
    uint8_t percent;
};
constexpr Point kCurve[] = {
    {1000, 0},  {1100, 8},  {1150, 15}, {1200, 27}, {1250, 42}, {1300, 58},
    {1350, 72}, {1400, 84}, {1450, 93}, {1500, 98}, {1550, 100},
};
constexpr uint8_t kCells = 4;

}  // namespace

bool init() { return adc::init(); }

uint32_t read_mv() {
    const uint32_t pin_mv = adc::to_mv(adc::read(ADC_CHANNEL_0));
    return pin_mv * (kDividerTop + kDividerBottom) / kDividerBottom;
}

uint8_t percent_from_mv(uint32_t pack_mv) {
    const uint32_t cell = pack_mv / kCells;
    constexpr uint8_t kLast = sizeof(kCurve) / sizeof(kCurve[0]) - 1;
    if (cell <= kCurve[0].cell_mv) {
        return 0;
    }
    if (cell >= kCurve[kLast].cell_mv) {
        return 100;
    }
    uint8_t i = 1;
    while (cell > kCurve[i].cell_mv) {
        ++i;
    }
    // Linear between the two neighboring points, then rounded to the nearest 5%: the estimate
    // isn't good to better than that.
    const Point& lo = kCurve[i - 1];
    const Point& hi = kCurve[i];
    const uint32_t pct = lo.percent + (cell - lo.cell_mv) * (hi.percent - lo.percent) /
                                          (hi.cell_mv - lo.cell_mv);
    return static_cast<uint8_t>((pct + 2) / 5 * 5);
}

}  // namespace battery
