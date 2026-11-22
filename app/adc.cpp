#include "adc.hpp"

#include "peripherals.hpp"

namespace adc {
namespace {

bool initialized = false;
uint32_t vdda = 3300;

bool select(uint32_t channel) {
    ADC_ChannelConfTypeDef config = {};
    config.Channel = channel;
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    return HAL_ADC_ConfigChannel(&hadc1, &config) == HAL_OK;
}

}  // namespace

bool init() {
    if (initialized) {
        return true;
    }
    // Long sample time: the photoresistor and battery dividers are high impedance, and
    // VREFINT needs at least 4 us (79.5 cycles at 8 MHz = ~10 us). CubeMX default is 1.5 cycles.
    hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_79CYCLES_5;
    if (HAL_ADC_Init(&hadc1) != HAL_OK || HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) {
        return false;
    }
    initialized = true;
    measure_vdda_mv();
    return true;
}

uint16_t read(uint32_t channel, uint8_t samples) {
    if (!initialized || !select(channel)) {
        return 0;
    }
    if (samples == 0) {
        samples = 1;
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK || HAL_ADC_PollForConversion(&hadc1, 5) != HAL_OK) {
            return 0;
        }
        sum += HAL_ADC_GetValue(&hadc1);
    }
    return static_cast<uint16_t>(sum / samples);
}

uint32_t measure_vdda_mv() {
    const uint16_t raw = read(ADC_CHANNEL_VREFINT);
    if (raw != 0) {
        vdda = __LL_ADC_CALC_VREFANALOG_VOLTAGE(raw, LL_ADC_RESOLUTION_12B);
    }
    return vdda;
}

uint32_t vdda_mv() { return vdda; }

uint32_t to_mv(uint16_t raw) { return raw * vdda / 4095u; }

}  // namespace adc
