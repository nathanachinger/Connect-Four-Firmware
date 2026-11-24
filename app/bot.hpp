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

// Longest expected line (principal variation) that is tracked: enough to fill one LCD line.
inline constexpr uint8_t kPvMax = 8;

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
    // Expected line, starting with the chosen move: the bot's move, the reply it expects, and so
    // on (columns 0-6). Empty for Easy. Ends early if the game ends.
    uint8_t pv[kPvMax];
    uint8_t pv_len;
};

// Chooses a column for the side to move. The game must not be over.
uint8_t choose(const game::Board& board, Level level, Rng& rng, const Hooks& hooks = {},
               Stats* stats = nullptr);

// A forced result scores kWinScore - plies (a loss: the negative), where plies counts half-moves
// from now up to and including the winning move. Anything at or beyond kWinThreshold is forced.
inline constexpr int32_t kWinScore = 1000000;
inline constexpr int32_t kWinThreshold = kWinScore - 64;

// Depth-limited negamax score for the side to move, exposed for testing.
int32_t search(const game::Board& board, uint8_t depth);

// Every column scored for the side to move by a full search `depth` plies deep (the move
// itself included), e.g. to show a player their options. Same scale as Stats::score.
inline constexpr int32_t kFullColumn = -2147483647 - 1;  // INT32_MIN: column can't be played
struct Analysis {
    int32_t scores[game::kCols];  // kFullColumn where the column is full
    int32_t best;                 // best of scores: the position's score for the side to move
    uint8_t best_col;             // the most central column with the best score
    uint8_t pv[kPvMax];           // expected line starting with best_col
    uint8_t pv_len;
};
// The game must not be over.
void analyze(const game::Board& board, uint8_t depth, Analysis& out);

}  // namespace bot
