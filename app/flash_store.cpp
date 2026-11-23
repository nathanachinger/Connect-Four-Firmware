#include "flash_store.hpp"

namespace flash_store {
namespace {

constexpr uint8_t kMagic = 0xC4;
constexpr uint64_t kErased = ~uint64_t{0};

// CRC-8 (poly 0x07) over the first 7 bytes.
uint8_t crc8(uint64_t record) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 7; ++i) {
        crc ^= static_cast<uint8_t>(record >> (8 * i));
        for (uint8_t b = 0; b < 8; ++b) {
            crc = static_cast<uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1);
        }
    }
    return crc;
}

// Slots fill from 0 upward, so the first erased one ends the used part.
uint32_t first_free(const Flash& flash) {
    uint32_t i = 0;
    while (i < kSlots && flash.page[i] != kErased) {
        ++i;
    }
    return i;
}

}  // namespace

uint64_t encode(uint8_t version, const uint8_t* payload) {
    uint64_t r = kMagic;
    r |= static_cast<uint64_t>(version) << 8;
    for (uint8_t i = 0; i < kPayloadBytes; ++i) {
        r |= static_cast<uint64_t>(payload[i]) << (8 * (2 + i));
    }
    return r | static_cast<uint64_t>(crc8(r)) << 56;
}

bool decode(uint64_t record, uint8_t version, uint8_t* out) {
    if (static_cast<uint8_t>(record) != kMagic || static_cast<uint8_t>(record >> 8) != version ||
        static_cast<uint8_t>(record >> 56) != crc8(record)) {
        return false;
    }
    for (uint8_t i = 0; i < kPayloadBytes; ++i) {
        out[i] = static_cast<uint8_t>(record >> (8 * (2 + i)));
    }
    return true;
}

bool load(const Flash& flash, uint8_t version, uint8_t* out) {
    // Newest first; skip anything that doesn't check out (e.g. a save cut off by power loss).
    for (uint32_t i = first_free(flash); i > 0; --i) {
        if (decode(flash.page[i - 1], version, out)) {
            return true;
        }
    }
    return false;
}

bool save(const Flash& flash, uint8_t version, const uint8_t* payload) {
    uint8_t current[kPayloadBytes];
    if (load(flash, version, current)) {
        bool same = true;
        for (uint8_t i = 0; i < kPayloadBytes; ++i) {
            same = same && current[i] == payload[i];
        }
        if (same) {
            return true;  // nothing to write: saves flash wear
        }
    }

    uint32_t slot = first_free(flash);
    if (slot == kSlots) {
        if (!flash.erase()) {
            return false;
        }
        slot = 0;
    }
    return flash.program(slot, encode(version, payload));
}

}  // namespace flash_store
