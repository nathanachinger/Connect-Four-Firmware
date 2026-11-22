#include "audio.hpp"

#include "peripherals.hpp"

namespace audio {
namespace {

constexpr uint16_t kMidScale = 2048;
constexpr uint32_t kFadesPerSecond = 200;  // each fade is 5 ms

constexpr int16_t kSine[64] = {
        0,   201,   399,   594,   783,   965,  1137,  1299,
     1447,  1582,  1702,  1805,  1891,  1959,  2008,  2037,
     2047,  2037,  2008,  1959,  1891,  1805,  1702,  1582,
     1447,  1299,  1137,   965,   783,   594,   399,   201,
        0,  -201,  -399,  -594,  -783,  -965, -1137, -1299,
    -1447, -1582, -1702, -1805, -1891, -1959, -2008, -2037,
    -2047, -2037, -2008, -1959, -1891, -1805, -1702, -1582,
    -1447, -1299, -1137,  -965,  -783,  -594,  -399,  -201,
};

// The timer clock equals PCLK because the APB prescaler is 1.
uint32_t sample_rate_hz() {
    return HAL_RCC_GetPCLK1Freq() / ((htim6.Init.Prescaler + 1) * (htim6.Init.Period + 1));
}

void start_sample_timer() {
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start(&htim6);
}

void stop_sample_timer() { HAL_TIM_Base_Stop(&htim6); }

void wait_for_sample() {
    while (__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE) == 0) {
    }
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
}

void write_dac(uint16_t value) { HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value); }

// Phase increment for a 32-bit accumulator whose top 6 bits index kSine.
uint32_t phase_step(uint32_t freq_hz, uint32_t rate_hz) {
    return static_cast<uint32_t>((static_cast<uint64_t>(freq_hz) << 32) / rate_hz);
}

uint16_t sample(uint32_t phase, uint32_t gain) {
    // gain is volume * envelope, scaled so 256 * 256 is full swing.
    const int32_t s = kSine[phase >> 26];
    return static_cast<uint16_t>(kMidScale + (s * static_cast<int32_t>(gain)) / 65536);
}

uint32_t envelope(uint32_t i, uint32_t total, uint32_t fade) {
    const uint32_t remaining = total - i;
    const uint32_t edge = i < remaining ? i : remaining;
    return edge >= fade ? 256 : edge * 256 / fade;
}

}  // namespace

void begin() {
    write_dac(kMidScale);
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
    HAL_Delay(5);
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(50);  // let the amplifier and output coupling caps settle
}

void end() {
    write_dac(kMidScale);
    HAL_Delay(5);
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_RESET);
    HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
}

void tone(uint32_t freq_hz, uint32_t duration_ms, uint16_t volume) {
    sweep(freq_hz, freq_hz, duration_ms, volume);
}

void sweep(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume) {
    if (volume > 256) {
        volume = 256;
    }
    const uint32_t rate = sample_rate_hz();
    const uint32_t fade = rate / kFadesPerSecond;
    const uint32_t total = rate * duration_ms / 1000;
    if (total == 0) {
        return;
    }

    start_sample_timer();
    uint32_t phase = 0;
    uint32_t step = 0;
    for (uint32_t i = 0; i < total; ++i) {
        // Recompute the step every 64 samples (4 ms); plenty smooth for a test sweep.
        if ((i & 63u) == 0) {
            const int64_t freq = start_hz + (static_cast<int64_t>(end_hz) - start_hz) *
                                                static_cast<int64_t>(i) / total;
            step = phase_step(static_cast<uint32_t>(freq), rate);
        }
        wait_for_sample();
        write_dac(sample(phase, volume * envelope(i, total, fade)));
        phase += step;
    }
    write_dac(kMidScale);
    stop_sample_timer();
}

void silence(uint32_t duration_ms) {
    write_dac(kMidScale);
    HAL_Delay(duration_ms);
}

}  // namespace audio
