#pragma once

#include <cstdint>

namespace audio {

// DAC1_OUT1 (PA4) -> PAM8302 amplifier, enabled by AMP_EN (PA2).
// Tones are generated in software: TIM6 paces samples (polled, no interrupts or DMA).
// The sample rate is whatever TIM6 is set to in CubeMX (16 kHz: period 3999 at 64 MHz).
// Each call blocks until the sound finishes.

// Starts the DAC at mid-scale, then turns the amplifier on.
void begin();
// Returns the DAC to mid-scale, then turns the amplifier off.
void end();

// Sine tone. volume: 0..256 (256 = full DAC swing). Short fades at each end avoid clicks.
void tone(uint32_t freq_hz, uint32_t duration_ms, uint16_t volume);
// Linear frequency sweep from start_hz to end_hz.
void sweep(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume);
void silence(uint32_t duration_ms);

}  // namespace audio
