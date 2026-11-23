#include "settings.hpp"

#include "backlight.hpp"
#include "flash_store.hpp"
#include "light_sensor.hpp"

namespace settings {
namespace {

// Auto brightness is deliberately slow so a hand moving over the board doesn't pump it:
//  1. A hand's shadow only ever makes the reading darker, so the room level is the brightest
//     reading of the last 2.5 s. Shadows shorter than that are ignored entirely; a
//     room that really got darker shows up once it has stayed dark that long.
//  2. That level is averaged (exponential average, sampled every 50 ms), quicker when the room
//     gets brighter than when it gets darker.
//  3. Changes smaller than kDeadband percent points are ignored, so it never creeps.
//  4. Once it does change, it fades toward the new level instead of jumping.
constexpr uint32_t kSampleMs = 50;
constexpr uint32_t kSlotMs = 500;       // the window is kept as the brightest reading per slot
constexpr uint8_t kWindowSlots = 5;     // 2.5 s window
constexpr int32_t kBrightenShift = 4;  // average over 2^4 samples: ~0.8 s
constexpr int32_t kDarkenShift = 5;    // average over 2^5 samples: ~1.6 s
constexpr int32_t kDeadband = 6;       // percent points
constexpr uint8_t kMinPercent = 10;     // keep the screen readable in the dark
constexpr uint8_t kMinLedPwm = 4;       // dimmest LED level that's still clearly lit
constexpr uint16_t kMaxGain = 128;      // half the DAC swing: loud, without clipping the amp

// Bump when the saved layout changes; older records are then ignored (defaults are used).
constexpr uint8_t kRecordVersion = 1;

Values g_values;
uint8_t g_auto_percent = 80;  // auto level being shown
uint8_t g_percent = 0;        // what the backlight and LEDs are set to now
uint16_t g_slot_max[kWindowSlots] = {};  // brightest reading in each recent slot
uint8_t g_slot = 0;                       // slot being filled
uint32_t g_slot_start_ms = 0;
int32_t g_light_avg = 0;      // averaged room level, 8 fractional bits
bool g_fading = false;        // moving toward a new auto level
uint32_t g_next_sample_ms = 0;

}  // namespace

Values& values() { return g_values; }

void load() {
    uint8_t p[flash_store::kPayloadBytes];
    if (!flash_store::load(flash_store::internal(), kRecordVersion, p)) {
        return;
    }
    Values v;
    v.auto_brightness = (p[0] & 1) != 0;
    v.brightness = p[1];
    v.volume = p[2];
    if (v.brightness >= 1 && v.brightness <= kMaxLevel && v.volume <= kMaxLevel) {
        g_values = v;
    }
}

bool save() {
    const uint8_t p[flash_store::kPayloadBytes] = {
        static_cast<uint8_t>(g_values.auto_brightness ? 1 : 0), g_values.brightness,
        g_values.volume, 0, 0};
    return flash_store::save(flash_store::internal(), kRecordVersion, p);
}

uint8_t led_pwm_for_percent(uint8_t percent) {
    const uint32_t pwm = 255u * percent * percent / 10000u;
    return static_cast<uint8_t>(pwm < kMinLedPwm ? kMinLedPwm : pwm);
}

uint8_t auto_percent_for_light(uint16_t raw) {
    // Brighter room -> brighter screen and LEDs, with a floor so they stay visible in the dark.
    return static_cast<uint8_t>(kMinPercent + raw * (100u - kMinPercent) / 4095u);
}

uint16_t gain_for_volume(uint8_t level) {
    if (level > kMaxLevel) {
        level = kMaxLevel;
    }
    // Squared, like brightness: loudness is perceived roughly logarithmically.
    return static_cast<uint16_t>(kMaxGain * level * level / (kMaxLevel * kMaxLevel));
}

void init() {
    const uint16_t raw = light::read_raw();
    for (uint16_t& m : g_slot_max) {
        m = raw;
    }
    g_light_avg = static_cast<int32_t>(raw) << 8;
    g_auto_percent = auto_percent_for_light(raw);
}

static void track_room_light(uint32_t now_ms) {
    const uint16_t raw = light::read_raw();
    if (now_ms - g_slot_start_ms >= kSlotMs) {
        g_slot_start_ms = now_ms;
        g_slot = static_cast<uint8_t>((g_slot + 1) % kWindowSlots);
        g_slot_max[g_slot] = raw;
    } else if (raw > g_slot_max[g_slot]) {
        g_slot_max[g_slot] = raw;
    }
    uint16_t room = 0;
    for (const uint16_t m : g_slot_max) {
        room = m > room ? m : room;
    }

    const int32_t diff = (static_cast<int32_t>(room) << 8) - g_light_avg;
    g_light_avg += diff >> (diff > 0 ? kBrightenShift : kDarkenShift);

    const int32_t target = auto_percent_for_light(static_cast<uint16_t>(g_light_avg >> 8));
    const int32_t error = target - g_auto_percent;
    if (!g_fading && (error >= kDeadband || error <= -kDeadband)) {
        g_fading = true;
    }
    if (g_fading) {
        // Stop only once caught up and the average has (nearly) reached the room level, so a
        // slow change finishes instead of stalling inside the deadband.
        const int32_t settle = 8 << 8;  // within 8 ADC counts
        if (error == 0 && diff < settle && diff > -settle) {
            g_fading = false;
        } else if (error != 0) {
            // About 1 point per sample, faster for big changes (lights switched on or off).
            const int32_t size = 1 + (error > 0 ? error : -error) / 16;
            g_auto_percent = static_cast<uint8_t>(g_auto_percent + (error > 0 ? size : -size));
        }
    }
}

void update(uint32_t now_ms) {
    // Tracks the room even in manual mode, so switching to AUTO starts at the right level.
    if (static_cast<int32_t>(now_ms - g_next_sample_ms) >= 0) {
        g_next_sample_ms = now_ms + kSampleMs;
        track_room_light(now_ms);
    }

    const uint8_t percent =
        g_values.auto_brightness ? g_auto_percent : static_cast<uint8_t>(g_values.brightness * 10);
    if (percent != g_percent) {
        g_percent = percent;
        backlight::set_percent(percent);
    }
}

uint8_t nearest_level() {
    const uint8_t percent = g_percent != 0 ? g_percent : g_auto_percent;
    const uint8_t level = static_cast<uint8_t>((percent + 5) / 10);
    return level < 1 ? 1 : (level > kMaxLevel ? kMaxLevel : level);
}

uint8_t led_pwm() { return led_pwm_for_percent(g_percent != 0 ? g_percent : g_auto_percent); }

uint16_t volume_gain() { return gain_for_volume(g_values.volume); }

}  // namespace settings
