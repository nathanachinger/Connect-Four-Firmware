#include "board.hpp"

namespace game {

uint64_t lines_of_four(uint64_t pieces) {
    // Shift distances: vertical, diagonal "\" (down-right), horizontal, diagonal "/" (up-right).
    constexpr uint8_t kDirections[] = {1, kColBits - 1, kColBits, kColBits + 1};
    uint64_t cells = 0;
    for (const uint8_t d : kDirections) {
        // Bit i of `starts` is set when bits i, i+d, i+2d and i+3d are all set.
        const uint64_t pairs = pieces & (pieces >> d);
        const uint64_t starts = pairs & (pairs >> (2 * d));
        cells |= starts | (starts << d) | (starts << (2 * d)) | (starts << (3 * d));
    }
    return cells;
}

void Board::reset(Player first) {
    pieces_[0] = pieces_[1] = 0;
    winning_ = 0;
    for (uint8_t& h : height_) {
        h = 0;
    }
    moves_ = 0;
    to_move_ = first;
    result_ = Result::InProgress;
}

bool Board::can_play(uint8_t col) const {
    return result_ == Result::InProgress && col < kCols && height_[col] < kRows;
}

int8_t Board::play(uint8_t col) {
    if (!can_play(col)) {
        return -1;
    }
    const uint8_t row = height_[col]++;
    const auto me = static_cast<uint8_t>(to_move_);
    pieces_[me] |= bit(row, col);
    ++moves_;

    winning_ = lines_of_four(pieces_[me]);
    if (winning_ != 0) {
        result_ = to_move_ == Player::Red ? Result::RedWins : Result::YellowWins;
    } else if (moves_ == kCols * kRows) {
        result_ = Result::Draw;
    }
    to_move_ = other(to_move_);
    return static_cast<int8_t>(row);
}

Cell Board::at(uint8_t row, uint8_t col) const {
    const uint64_t b = bit(row, col);
    if (pieces_[0] & b) {
        return Cell::Red;
    }
    if (pieces_[1] & b) {
        return Cell::Yellow;
    }
    return Cell::Empty;
}

}  // namespace game
