#pragma once

#include <cstdint>

namespace game {

// Standard Connect Four: 7 columns x 6 rows. Pieces drop to the lowest empty row.
inline constexpr uint8_t kCols = 7;
inline constexpr uint8_t kRows = 6;

enum class Player : uint8_t { Red, Yellow };
enum class Cell : uint8_t { Empty, Red, Yellow };
enum class Result : uint8_t { InProgress, RedWins, YellowWins, Draw };

inline Player other(Player p) { return p == Player::Red ? Player::Yellow : Player::Red; }

// Bitboard layout: bit (col * 7 + row), row 0 = bottom. Each column has a 7th, always-empty bit
// so shifts for horizontal and diagonal lines can't wrap from one column into the next.
inline constexpr uint8_t kColBits = kRows + 1;
inline constexpr uint64_t bit(uint8_t row, uint8_t col) { return uint64_t{1} << (col * kColBits + row); }

// Small enough (~24 bytes) to copy freely, e.g. for bot search.
class Board {
public:
    explicit Board(Player first = Player::Red) { reset(first); }

    void reset(Player first = Player::Red);

    // True if the game is still on and the column has room.
    bool can_play(uint8_t col) const;
    // Drops a piece for the side to move. Returns the landing row (0 = bottom), or -1 if the
    // move is illegal (column full, out of range, or game over).
    int8_t play(uint8_t col);

    Player to_move() const { return to_move_; }
    uint8_t moves() const { return moves_; }
    uint8_t height(uint8_t col) const { return height_[col]; }
    Result result() const { return result_; }
    bool over() const { return result_ != Result::InProgress; }

    // row 0 = bottom.
    Cell at(uint8_t row, uint8_t col) const;
    uint64_t pieces(Player p) const { return pieces_[static_cast<uint8_t>(p)]; }

    // Every cell that is part of a winning line (more than 4 if the last move made several).
    // 0 unless someone has won.
    uint64_t winning_cells() const { return winning_; }

private:
    uint64_t pieces_[2];
    uint64_t winning_;
    uint8_t height_[kCols];
    uint8_t moves_;
    Player to_move_;
    Result result_;
};

// All cells that belong to a line of 4 or more in `pieces` (0 if none).
uint64_t lines_of_four(uint64_t pieces);

}  // namespace game
