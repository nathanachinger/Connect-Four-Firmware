#pragma once

#include <cstdint>

namespace game {

// Standard Connect Four board. The physical LED grid is 7x7: one cursor row on top + these rows.
inline constexpr uint8_t kColumns = 7;
inline constexpr uint8_t kRows = 6;
inline constexpr uint8_t kConnect = 4;

}  // namespace game
