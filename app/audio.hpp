#pragma once

#include <cstdint>

namespace audio {

// DAC1_OUT1 (PA4) -> PAM8302 amplifier, enabled by AMP_EN (PA2).
// TIM6 TRGO triggers a DAC conversion every sample; DMA1 Channel 1 feeds the DAC from a circular
// buffer. The half/complete DMA interrupts synthesize the next half buffer, so playback runs in the
// background. Between begin() and end() the stream runs continuously, outputting mid-scale when idle.

inline constexpr uint32_t kSampleRateHz = 16000;

// Starts the sample stream at mid-scale, then turns the amplifier on.
void begin();
// Stops any sound, lets the output settle at mid-scale, turns the amplifier off, stops the stream.
void end();

// Non-blocking. Replaces whatever is playing; it starts within one buffer (~16 ms).
// volume: 0..256 (256 = full DAC swing). Short fades at each end avoid clicks.
void play_tone(uint32_t freq_hz, uint32_t duration_ms, uint16_t volume);
// Linear frequency sweep from start_hz to end_hz.
void play_sweep(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume);
// Fades the current sound out over a few ms (no click).
void stop();
// True until the last sample of the current sound has left the DAC.
bool is_playing();
// Blocks until is_playing() is false.
void wait();

// Blocking helpers: play, then wait for it to finish.
void tone(uint32_t freq_hz, uint32_t duration_ms, uint16_t volume);
void sweep(uint32_t start_hz, uint32_t end_hz, uint32_t duration_ms, uint16_t volume);
// Waits for the current sound, then stays silent for duration_ms.
void silence(uint32_t duration_ms);

}  // namespace audio
