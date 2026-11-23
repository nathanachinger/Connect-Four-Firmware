#include "bot.hpp"

// Negamax with alpha-beta on a (current player, occupied) bitboard pair, using the same
// 7-bits-per-column layout as game::Board. Each node first looks for an immediate win, then
// restricts itself to moves that don't lose at once: block the opponent's single threat (two
// threats = lost), and never play directly under an opponent's winning cell.

namespace bot {
namespace {

using game::kColBits;
using game::kCols;
using game::kRows;

constexpr int32_t kWin = 1000000;
constexpr int32_t kInfinity = kWin + 1;
constexpr uint8_t kMaxMoves = kCols * kRows;

constexpr uint64_t bottom_row() {
    uint64_t m = 0;
    for (uint8_t c = 0; c < kCols; ++c) {
        m |= uint64_t{1} << (c * kColBits);
    }
    return m;
}
constexpr uint64_t kBottom = bottom_row();
constexpr uint64_t kBoard = kBottom * ((uint64_t{1} << kRows) - 1);

constexpr uint64_t column(uint8_t c) { return ((uint64_t{1} << kRows) - 1) << (c * kColBits); }

// Center first: central columns take part in the most lines, so they are usually best, and
// trying good moves first lets alpha-beta prune more.
constexpr uint8_t kOrder[kCols] = {3, 2, 4, 1, 5, 0, 6};

// Lowest empty cell of every column that isn't full.
inline uint64_t playable(uint64_t mask) { return (mask + kBottom) & kBoard; }

// Empty cells that would complete a line of four for `stones`.
uint64_t winning_cells(uint64_t stones, uint64_t mask) {
    // Vertical: three stones directly below.
    uint64_t r = (stones << 1) & (stones << 2) & (stones << 3);

    // The other three directions: the empty cell can be at any of the 4 positions in the line.
    constexpr uint8_t kShifts[] = {kColBits - 1, kColBits, kColBits + 1};  // "\", -, "/"
    for (const uint8_t d : kShifts) {
        uint64_t p = (stones << d) & (stones << (2 * d));
        r |= p & (stones << (3 * d));
        r |= p & (stones >> d);
        p = (stones >> d) & (stones >> (2 * d));
        r |= p & (stones << d);
        r |= p & (stones >> (3 * d));
    }
    return r & (kBoard ^ mask);
}

inline uint32_t popcount32(uint32_t x) {
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
}
inline int32_t popcount(uint64_t x) {
    return static_cast<int32_t>(popcount32(static_cast<uint32_t>(x)) +
                                popcount32(static_cast<uint32_t>(x >> 32)));
}

// Positional weight of each cell = number of lines of four through it (3 at the corners,
// 13 in the middle). Stored as 4 bit-planes so a weighted sum is 4 popcounts.
constexpr uint8_t lines_through(int row, int col) {
    const int dr[] = {0, 1, 1, 1};
    const int dc[] = {1, 0, 1, -1};
    uint8_t n = 0;
    for (int d = 0; d < 4; ++d) {
        for (int start = -3; start <= 0; ++start) {
            bool fits = true;
            for (int k = 0; k < 4; ++k) {
                const int r = row + dr[d] * (start + k);
                const int c = col + dc[d] * (start + k);
                fits = fits && r >= 0 && r < kRows && c >= 0 && c < kCols;
            }
            n = static_cast<uint8_t>(n + (fits ? 1 : 0));
        }
    }
    return n;
}
constexpr uint64_t weight_plane(int b) {
    uint64_t m = 0;
    for (int c = 0; c < kCols; ++c) {
        for (int r = 0; r < kRows; ++r) {
            if ((lines_through(r, c) >> b) & 1) {
                m |= uint64_t{1} << (c * kColBits + r);
            }
        }
    }
    return m;
}
constexpr uint64_t kWeight[4] = {weight_plane(0), weight_plane(1), weight_plane(2), weight_plane(3)};
static_assert(lines_through(0, 0) == 3 && lines_through(2, 3) == 13, "line count table");

int32_t positional(uint64_t stones) {
    return popcount(stones & kWeight[0]) + 2 * popcount(stones & kWeight[1]) +
           4 * popcount(stones & kWeight[2]) + 8 * popcount(stones & kWeight[3]);
}

// Static score for the side to move. Threats (empty cells that would complete four) dominate;
// cell weights break ties and steer the opening toward the center.
constexpr int32_t kThreatWeight = 24;
int32_t evaluate(uint64_t cur, uint64_t mask, uint64_t my_threats, uint64_t their_threats) {
    return kThreatWeight * (popcount(my_threats) - popcount(their_threats)) + positional(cur) -
           positional(cur ^ mask);
}

class Searcher {
public:
    Searcher(const Hooks& hooks, uint32_t deadline_ms, bool timed)
        : hooks_(hooks), deadline_ms_(deadline_ms), timed_(timed) {}

    int32_t negamax(uint64_t cur, uint64_t mask, uint8_t moves, uint8_t depth, int32_t alpha,
                    int32_t beta, uint8_t ply);

    uint32_t nodes = 0;
    bool aborted = false;
    // Only the first iteration of a timed search must always finish, so we always have a move.
    bool may_abort = false;

private:
    void poll();

    Hooks hooks_;  // by value: callers may pass a temporary
    uint32_t deadline_ms_;
    bool timed_;
    uint32_t next_poll_ = kPollNodes;
    static constexpr uint32_t kPollNodes = 2048;
};

void Searcher::poll() {
    next_poll_ = nodes + kPollNodes;
    if (hooks_.progress != nullptr && !hooks_.progress(hooks_.ctx)) {
        aborted = true;  // the caller wants out; honored even in the first iteration
    }
    if (may_abort && timed_ && hooks_.now_ms != nullptr &&
        static_cast<int32_t>(hooks_.now_ms() - deadline_ms_) >= 0) {
        aborted = true;
    }
}

int32_t Searcher::negamax(uint64_t cur, uint64_t mask, uint8_t moves, uint8_t depth,
                          int32_t alpha, int32_t beta, uint8_t ply) {
    if (++nodes >= next_poll_) {
        poll();
    }
    if (aborted) {
        return 0;
    }
    if (moves == kMaxMoves) {
        return 0;  // full board, nobody won
    }

    const uint64_t open = playable(mask);
    const uint64_t mine = winning_cells(cur, mask);
    if (mine & open) {
        return kWin - ply - 1;  // win with this move
    }

    const uint64_t theirs = winning_cells(cur ^ mask, mask);
    uint64_t candidates = open;
    const uint64_t forced = open & theirs;
    if (forced) {
        if (forced & (forced - 1)) {
            return -(kWin - ply - 2);  // two threats: can only block one
        }
        candidates = forced;
    }
    candidates &= ~(theirs >> 1);  // don't give them the cell above
    if (candidates == 0) {
        return -(kWin - ply - 2);
    }
    if (depth == 0) {
        return evaluate(cur, mask, mine, theirs);
    }

    int32_t best = -kInfinity;
    for (const uint8_t col : kOrder) {
        const uint64_t move = candidates & column(col);
        if (move == 0) {
            continue;
        }
        const int32_t score = -negamax(cur ^ mask, mask | move, static_cast<uint8_t>(moves + 1),
                                       static_cast<uint8_t>(depth - 1), -beta, -alpha,
                                       static_cast<uint8_t>(ply + 1));
        if (aborted) {
            return 0;
        }
        if (score > best) {
            best = score;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }
    return best;
}

struct Position {
    uint64_t cur;
    uint64_t mask;
    uint8_t moves;
};

Position position_of(const game::Board& board) {
    const uint64_t cur = board.pieces(board.to_move());
    return {cur, cur | board.pieces(game::other(board.to_move())), board.moves()};
}

// One full-width search of every root move. Scores are exact for every move tied with the best
// (searched with alpha = best - 1), so ties can be broken at random. Returns the best score and
// fills `best_cols` with the tied columns.
int32_t search_root(Searcher& s, const Position& p, uint8_t depth, const uint8_t* order,
                    uint8_t* best_cols, uint8_t& best_count) {
    best_count = 0;
    // Children assume the previous move didn't win, so take immediate wins here.
    const uint64_t wins = playable(p.mask) & winning_cells(p.cur, p.mask);
    if (wins != 0) {
        for (uint8_t c = 0; c < kCols; ++c) {
            if (wins & column(c)) {
                best_cols[best_count++] = c;
            }
        }
        return kWin - 1;
    }

    int32_t best = -kInfinity;
    for (uint8_t i = 0; i < kCols; ++i) {
        const uint8_t col = order[i];
        const uint64_t move = playable(p.mask) & column(col);
        if (move == 0) {
            continue;
        }
        const int32_t alpha = best > -kInfinity ? best - 1 : -kInfinity;
        const int32_t score = -s.negamax(p.cur ^ p.mask, p.mask | move,
                                         static_cast<uint8_t>(p.moves + 1),
                                         static_cast<uint8_t>(depth - 1), -kInfinity, -alpha, 1);
        if (s.aborted) {
            return best;
        }
        if (score > best) {
            best = score;
            best_count = 0;
        }
        if (score == best) {
            best_cols[best_count++] = col;
        }
    }
    return best;
}

// Easy: win if possible, else block, else random.
uint8_t choose_easy(const Position& p, Rng& rng) {
    const uint64_t open = playable(p.mask);
    uint64_t pick = open & winning_cells(p.cur, p.mask);
    if (pick == 0) {
        pick = open & winning_cells(p.cur ^ p.mask, p.mask);
    }
    if (pick == 0) {
        pick = open;
    }
    uint8_t cols[kCols];
    uint8_t n = 0;
    for (uint8_t c = 0; c < kCols; ++c) {
        if (pick & column(c)) {
            cols[n++] = c;
        }
    }
    return cols[rng.below(n)];
}

}  // namespace

uint32_t Rng::next() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
}

int32_t search(const game::Board& board, uint8_t depth) {
    const Position p = position_of(board);
    Searcher s({}, 0, false);
    return s.negamax(p.cur, p.mask, p.moves, depth, -kInfinity, kInfinity, 0);
}

uint8_t choose(const game::Board& board, Level level, Rng& rng, const Hooks& hooks,
               Stats* stats) {
    const Position p = position_of(board);
    const uint32_t start_ms = hooks.now_ms != nullptr ? hooks.now_ms() : 0;
    Stats st = {};

    uint8_t choice = 0;
    if (level == Level::Easy) {
        choice = choose_easy(p, rng);
    } else {
        const bool timed = level == Level::Hard && hooks.now_ms != nullptr;
        const uint8_t remaining = static_cast<uint8_t>(kMaxMoves - p.moves);
        const uint8_t max_depth = timed ? remaining : kMediumDepth;
        Searcher s(hooks, start_ms + kHardBudgetMs, timed);

        uint8_t order[kCols];
        for (uint8_t i = 0; i < kCols; ++i) {
            order[i] = kOrder[i];
        }
        uint8_t best_cols[kCols];
        uint8_t best_count = 0;
        // Fixed-depth levels go straight to their depth; Hard deepens one ply at a time.
        for (uint8_t depth = timed ? 1 : max_depth; depth <= max_depth; ++depth) {
            uint8_t cols[kCols];
            uint8_t count = 0;
            const int32_t score = search_root(s, p, depth, order, cols, count);
            if (s.aborted) {
                break;
            }
            best_count = count;
            for (uint8_t i = 0; i < count; ++i) {
                best_cols[i] = cols[i];
            }
            st.depth = depth;
            st.score = score;
            s.may_abort = true;

            // A certain result won't change with more depth.
            if (score >= kWinThreshold || score <= -kWinThreshold) {
                break;
            }
            if (timed) {
                // The next ply takes several times longer; don't start what can't finish.
                const uint32_t elapsed = hooks.now_ms() - start_ms;
                if (elapsed * 3 >= kHardBudgetMs) {
                    break;
                }
                // Search the current best first next time.
                const uint8_t lead = best_cols[0];
                uint8_t j = 0;
                order[j++] = lead;
                for (const uint8_t c : kOrder) {
                    if (c != lead) {
                        order[j++] = c;
                    }
                }
            }
        }
        st.nodes = s.nodes;
        st.aborted = s.aborted && best_count == 0;
        if (best_count > 0) {
            choice = best_cols[rng.below(best_count)];
        } else {
            // Aborted before the first depth finished: any legal move.
            for (const uint8_t c : kOrder) {
                if (board.can_play(c)) {
                    choice = c;
                    break;
                }
            }
        }
    }

    if (hooks.now_ms != nullptr) {
        st.ms = hooks.now_ms() - start_ms;
    }
    if (stats != nullptr) {
        *stats = st;
    }
    return choice;
}

}  // namespace bot
