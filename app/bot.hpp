#pragma once

#include <cstdint>

#include "board.hpp"

namespace bot {

// Easy:   random, but always takes a winning move and blocks an immediate loss.
// Medium: 4-ply search.
// Hard:   iterative deepening for up to kHardBudgetMs (stops early once the result is certain).
enum class Level : uint8_t { Easy, Medium, Hard };

inline constexpr uint8_t kMediumDepth = 4;
inline constexpr uint32_t kHardBudgetMs = 2000;

// xorshift32. Seed it from something unpredictable (ADC noise, button timing).
class Rng {
public:
    explicit Rng(uint32_t seed = 1) { reseed(seed); }
    void reseed(uint32_t seed) { state_ = seed != 0 ? seed : 0x9E3779B9u; }
    uint32_t next();
    // Uniform-ish in [0, n). n must be > 0.
    uint32_t below(uint32_t n) { return next() % n; }

private:
    uint32_t state_;
};

struct Hooks {
    // Millisecond clock. Needed for Hard's time budget; without it Hard searches to kMediumDepth.
    uint32_t (*now_ms)() = nullptr;
    // Called every few thousand nodes (several times a second) so the caller can animate and
    // read input. Return false to abort the search.
    bool (*progress)(void* ctx) = nullptr;
    void* ctx = nullptr;
};

struct Stats {
    uint32_t nodes;
    uint32_t ms;     // 0 without a clock
    uint8_t depth;   // deepest completed search (0 for Easy)
    int32_t score;   // from the bot's point of view; > 0 is good
    bool aborted;    // progress() stopped it before any depth finished; the move is just legal
};

// Chooses a column for the side to move. The game must not be over.
uint8_t choose(const game::Board& board, Level level, Rng& rng, const Hooks& hooks = {},
               Stats* stats = nullptr);

// Scores at or beyond this mean a forced win (negative: forced loss) within the search horizon.
inline constexpr int32_t kWinThreshold = 1000000 - 64;

// Depth-limited negamax score for the side to move, exposed for testing.
int32_t search(const game::Board& board, uint8_t depth);

}  // namespace bot
