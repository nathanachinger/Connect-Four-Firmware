#include <gtest/gtest.h>

#include "rules.hpp"

// Physical LED grid is 7x7: one cursor row above the playing field.
TEST(Rules, BoardFitsLedGrid) {
    EXPECT_EQ(game::kColumns, 7);
    EXPECT_EQ(game::kRows + 1, 7);
}
