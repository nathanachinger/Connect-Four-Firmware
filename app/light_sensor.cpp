#include "light_sensor.hpp"

#include "adc.hpp"
#include "peripherals.hpp"

namespace light {

bool init() { return adc::init(); }

uint16_t read_raw(uint8_t samples) { return adc::read(ADC_CHANNEL_8, samples); }

uint32_t raw_to_mv(uint16_t raw) { return adc::to_mv(raw); }

}  // namespace light
