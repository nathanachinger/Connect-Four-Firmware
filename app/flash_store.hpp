#pragma once

#include <cstdint>

namespace flash_store {

// A tiny EEPROM stand-in in the last 2 KB flash page (STM32G071C8: 64 KB, page 31).
// Each save appends one 8-byte record; the page is erased only when all 256 slots are used, so
// the page's ~10k erase cycles last for millions of saves. At boot the newest valid record wins.
// The linker check in cmake/settings_page.ld fails the build if code ever grows into the page.
inline constexpr uint32_t kPageAddress = 0x0800F800;
inline constexpr uint32_t kPageBytes = 2048;
inline constexpr uint32_t kSlots = kPageBytes / 8;

// Record: [magic][version][payload x5][crc8]. Erased flash (all 0xFF) never passes the checks.
inline constexpr uint8_t kPayloadBytes = 5;

// Raw page access, swappable so the logic can be tested off-target.
struct Flash {
    const volatile uint64_t* page;                // kSlots doublewords; flash changes under us
    bool (*erase)();                              // whole page to 0xFF
    bool (*program)(uint32_t slot, uint64_t value);  // one erased slot
};
// The real page, through the HAL.
const Flash& internal();

// Newest valid payload with this version. Returns false if there is none (blank page, older
// layout, or corruption), leaving `out` untouched.
bool load(const Flash& flash, uint8_t version, uint8_t* out);
// Appends a record unless the newest one already holds this payload. Returns false on a flash
// error; the previous record is still there in that case.
bool save(const Flash& flash, uint8_t version, const uint8_t* payload);

// Exposed for testing.
uint64_t encode(uint8_t version, const uint8_t* payload);
bool decode(uint64_t record, uint8_t version, uint8_t* out);

}  // namespace flash_store
