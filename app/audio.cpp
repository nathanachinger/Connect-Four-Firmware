#include "audio.hpp"

#include "peripherals.hpp"

namespace audio {
namespace {

constexpr uint16_t kMidScale = 2048;
constexpr uint32_t kFadeSamples = kSampleRateHz / 200;  // 5 ms

// Circular DMA buffer; each half is refilled while the other plays.
constexpr uint32_t kBufferSamples = 256;
constexpr uint32_t kHalfSamples = kBufferSamples / 2;
constexpr uint32_t kBufferMs = kBufferSamples * 1000 / kSampleRateHz;  // 16 ms

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

uint16_t g_buffer[kBufferSamples];

// Owned by the DMA interrupt while playing; the main thread writes it only with interrupts off.
struct Voice {
    uint32_t phase;
    uint32_t step;      // phase increment per sample
    int32_t step_inc;   // added to step every sample (frequency sweep)
    uint32_t index;     // samples generated so far
    uint32_t total;     // samples in this sound
    uint32_t volume;    // 0..256
};

Voice g_voice;
// Set by start(), cleared by the interrupt once the last sample is generated.
volatile bool g_active = false;
// Half-buffer fills still to play out after the voice ends (see fill()).
volatile uint8_t g_drain = 0;
bool g_running = false;

// Phase increment for a 32-bit accumulator whose top 6 bits index kSine.
uint32_t phase_step(uint32_t freq_hz) {
    return static_cast<uint32_t>((static_cast<uint64_t>(freq_hz) << 32) / kSampleRateHz);
}

uint32_t envelope(uint32_t i, uint32_t total) {
    const uint32_t remaining = total - i;
    const uint32_t edge = i < remaining ? i : remaining;
    return edge >= kFadeSamples ? 256 : edge * 256 / kFadeSamples;
}

uint16_t sample(uint32_t phase, uint32_t gain) {
    // gain is volume * envelope, scaled so 256 * 256 is full swing.
    const int32_t s = kSine[phase >> 26];
    return static_cast<uint16_t>(kMidScale + (s * static_cast<int32_t>(gain)) / 65536);
}

// Called from the DMA interrupt for the half that just finished playing.
void fill(uint16_t* out) {
    uint32_t n = 0;
    if (g_active) {
        Voice v = g_voice;
        for (; n < kHalfSamples && v.index < v.total; ++n, ++v.index) {
            out[n] = sample(v.phase, v.volume * envelope(v.index, v.total));
            v.phase += v.step;
            v.step += static_cast<uint32_t>(v.step_inc);
        }
        if (v.index >= v.total) {
            g_active = false;
            // This half plays after the other one does; the second idle fill from now is when
            // DMA has finished it.
            g_drain = 2;
        }
        g_voice = v;
    } else if (g_drain > 0) {
        g_drain = g_drain - 1;
    }
    for (; n < kHalfSamples; ++n) {
        out[n] = kMidScale;
    }
}

void start(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume) {
    if (!g_running) {
        return;
    }
    if (volume > 256) {
        volume = 256;
    }
    const uint32_t total = kSampleRateHz * duration_ms / 1000;
    if (total == 0) {
        return;
    }
    const uint32_t step0 = phase_step(start_hz);
    const uint32_t step1 = phase_step(end_hz);
    // Steps are < 2^31 for frequencies below Nyquist, so the difference fits in int32.
    const int32_t inc = static_cast<int32_t>(
        (static_cast<int64_t>(step1) - static_cast<int64_t>(step0)) / static_cast<int64_t>(total));

    const Voice v{0, step0, inc, 0, total, volume};
    __disable_irq();
    g_voice = v;
    g_active = true;
    g_drain = 0;
    __enable_irq();
}

}  // namespace

void begin() {
    if (g_running) {
        return;
    }
    for (uint16_t& s : g_buffer) {
        s = kMidScale;
    }
    g_active = false;
    g_drain = 0;

    __HAL_TIM_SET_AUTORELOAD(&htim6, SystemCoreClock / kSampleRateHz - 1);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, reinterpret_cast<const uint32_t*>(g_buffer),
                      kBufferSamples, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(&htim6);
    g_running = true;

    HAL_Delay(5);  // DAC output reaches mid-scale on the first trigger
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_SET);
    HAL_Delay(50);  // let the amplifier and output coupling caps settle
}

void end() {
    if (!g_running) {
        return;
    }
    stop();
    wait();
    HAL_Delay(5);
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_RESET);
    HAL_TIM_Base_Stop(&htim6);
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    g_running = false;
}

void play_tone(uint32_t freq_hz, uint32_t duration_ms, uint16_t volume) {
    start(freq_hz, freq_hz, duration_ms, volume);
}

void play_sweep(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume) {
    start(start_hz, end_hz, duration_ms, volume);
}

void stop() {
    __disable_irq();
    if (g_active && g_voice.total - g_voice.index > kFadeSamples) {
        // envelope() ramps down over the last kFadeSamples, so shortening total fades out.
        g_voice.total = g_voice.index + kFadeSamples;
    }
    __enable_irq();
}

bool is_playing() { return g_running && (g_active || g_drain > 0); }

void wait() {
    // Guard against a stalled DMA stream: no sound outlives its own length by this much.
    const uint32_t start_ms = HAL_GetTick();
    const uint32_t limit_ms = g_voice.total * 1000 / kSampleRateHz + 4 * kBufferMs;
    while (is_playing() && HAL_GetTick() - start_ms <= limit_ms) {
    }
}

void tone(uint32_t freq_hz, uint32_t duration_ms, uint16_t volume) {
    play_tone(freq_hz, duration_ms, volume);
    wait();
}

void sweep(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume) {
    play_sweep(start_hz, end_hz, duration_ms, volume);
    wait();
}

void silence(uint32_t duration_ms) {
    wait();
    HAL_Delay(duration_ms);
}

}  // namespace audio

extern "C" void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef*) {
    audio::fill(&audio::g_buffer[0]);
}

extern "C" void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef*) {
    audio::fill(&audio::g_buffer[audio::kHalfSamples]);
}
