#include "sound.hpp"

#include "audio.hpp"
#include "settings.hpp"

namespace sound {
namespace {

// Short, so the amp isn't left on amplifying supply noise (e.g. LED updates when the cursor
// blinks) between sounds. The cost: a sound after a pause waits ~55 ms for the amp to power up.
constexpr uint32_t kAmpIdleMs = 300;

// One note: a sweep from `from` to `to` Hz (equal for a plain tone), or a rest when from == 0.
struct Note {
    uint16_t from;
    uint16_t to;
    uint16_t ms;
};

struct Effect {
    const Note* notes;
    uint8_t count;
    uint8_t priority;  // higher wins
    uint8_t level;     // percent of the sound gain, so ticks sit under clicks and jingles
};

// Pitches (Hz)
constexpr uint16_t C4 = 262, E4 = 330, G4 = 392, B3 = 247;
constexpr uint16_t C5 = 523, E5 = 659, G5 = 784, C6 = 1047, G6 = 1568;

constexpr Note kMenuMove[] = {{1600, 1600, 12}};
constexpr Note kMenuSelect[] = {{C6, C6, 25}, {G6, G6, 45}};
constexpr Note kMenuBack[] = {{G6, G6, 25}, {C6, C6, 45}};
constexpr Note kAim[] = {{1200, 1200, 10}};
constexpr Note kDrop[] = {{900, 450, 200}};  // falling whistle; the landing cuts it off
constexpr Note kLand[] = {{240, 140, 45}};
constexpr Note kBlocked[] = {{300, 240, 110}};
// Results start with the landing thud, since they replace it.
constexpr Note kWin[] = {{240, 140, 45}, {0, 0, 60}, {C5, C5, 90}, {E5, E5, 90},
                         {G5, G5, 90},   {C6, C6, 260}};
constexpr Note kLose[] = {{240, 140, 45}, {0, 0, 60}, {G4, G4, 150}, {E4, E4, 150},
                          {C4, B3, 340}};
constexpr Note kDraw[] = {{240, 140, 45}, {0, 0, 60}, {C5, C5, 120}, {0, 0, 50}, {C5, C5, 160}};
constexpr Note kSoundOn[] = {{C6, C6, 40}, {G6, G6, 70}};

#define EFFECT(notes, priority, level) {notes, sizeof(notes) / sizeof(notes[0]), priority, level}
constexpr Effect kEffects[] = {
    EFFECT(kMenuMove, 0, 45),   EFFECT(kMenuSelect, 1, 70), EFFECT(kMenuBack, 1, 70),
    EFFECT(kAim, 0, 40),        EFFECT(kDrop, 1, 55),       EFFECT(kLand, 1, 100),
    EFFECT(kBlocked, 1, 70),    EFFECT(kWin, 2, 90),        EFFECT(kLose, 2, 90),
    EFFECT(kDraw, 2, 90),       EFFECT(kSoundOn, 1, 90),
};
#undef EFFECT
static_assert(sizeof(kEffects) / sizeof(kEffects[0]) == static_cast<uint8_t>(Id::SoundOn) + 1,
              "one entry per sound::Id, in order");

const Effect* g_effect = nullptr;  // playing, or nullptr
uint8_t g_next = 0;                // next note index
uint32_t g_next_ms = 0;            // when the next note (or the end) is due
bool g_amp_on = false;
uint32_t g_last_sound_ms = 0;

bool due(uint32_t now_ms, uint32_t at_ms) { return static_cast<int32_t>(now_ms - at_ms) >= 0; }

void start_note(const Note& n, uint32_t now_ms) {
    if (n.from != 0) {
        const auto gain = static_cast<uint16_t>(settings::sound_gain() * g_effect->level / 100);
        audio::play_sweep(n.from, n.to, n.ms, gain);
    }
    g_next_ms = now_ms + n.ms;
    g_last_sound_ms = g_next_ms;
}

}  // namespace

void play(Id id, uint32_t now_ms) {
    if (settings::sound_gain() == 0) {
        return;
    }
    const Effect& e = kEffects[static_cast<uint8_t>(id)];
    if (g_effect != nullptr && e.priority < g_effect->priority) {
        return;  // don't cut off something more important
    }
    if (!g_amp_on) {
        audio::begin();  // blocks ~55 ms while the amp powers up
        g_amp_on = true;
    }
    g_effect = &e;
    g_next = 1;
    start_note(e.notes[0], now_ms);
}

void update(uint32_t now_ms) {
    if (g_effect != nullptr && due(now_ms, g_next_ms)) {
        if (g_next < g_effect->count) {
            start_note(g_effect->notes[g_next++], now_ms);
        } else {
            g_effect = nullptr;
        }
    }
    if (g_amp_on && g_effect == nullptr && !audio::is_playing() &&
        due(now_ms, g_last_sound_ms + kAmpIdleMs)) {
        audio::end();
        g_amp_on = false;
    }
}

void stop() {
    g_effect = nullptr;
    if (g_amp_on) {
        audio::end();
        g_amp_on = false;
    }
}

}  // namespace sound
