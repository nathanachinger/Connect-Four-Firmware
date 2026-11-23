#pragma once

#include <cstdint>

namespace sound {

// Sound effects on top of the DMA audio driver. Each effect is a short sequence of notes,
// played in the background whenever sound effects are on in settings.
//
// Priorities keep important sounds from being cut off: a new sound only interrupts one of the
// same or lower priority (turning ticks < clicks < game results).
//
// The amp is switched on for the first sound and off after kAmpIdleMs (0.3 s) of quiet, so it
// isn't powered (or picking up noise) while nothing plays. Switching it on takes ~55 ms, so the first sound
// after a pause starts that much late.
enum class Id : uint8_t {
    MenuMove,       // highlight moved
    MenuSelect,     // item chosen, or play again
    MenuBack,       // hold: back / leave a game
    Aim,            // game cursor moved one column
    Drop,           // piece released
    Land,           // piece hit the stack
    Blocked,        // nothing happened: full column or unavailable item
    Win,            // you won (or someone won in 2 player)
    Lose,           // the bot won
    Draw,
    SoundOn,        // settings: sound effects switched on
};

void play(Id id, uint32_t now_ms);
// Call every loop (and during long blocking work): advances note sequences and powers the amp
// down when idle.
void update(uint32_t now_ms);
// Silences everything and turns the amp off now (e.g. before shutdown).
void stop();

}  // namespace sound
