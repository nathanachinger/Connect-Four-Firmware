// HAL backend for flash_store: erase and program the reserved page in internal flash.
// Kept apart from flash_store.cpp so the record logic builds and tests off-target.

#include "flash_store.hpp"

#include "peripherals.hpp"

namespace flash_store {
namespace {

constexpr uint32_t kPage = (kPageAddress - FLASH_BASE) / kPageBytes;  // 31 on a 64 KB part

bool erase_page() {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = {};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.Page = kPage;
    erase.NbPages = 1;
    uint32_t bad_page = 0;
    const bool ok = HAL_FLASHEx_Erase(&erase, &bad_page) == HAL_OK;  // ~22 ms, CPU stalls
    HAL_FLASH_Lock();
    return ok;
}

// The same steps as the HAL's doubleword path in HAL_FLASH_Program(). That function also has a
// "fast program" branch that must run from RAM, and calling it links that code into .data
// (more RAM used, and an RWX-segment linker warning) even though it's never used here.
bool program_slot(uint32_t slot, uint64_t value) {
    if (slot >= kSlots) {
        return false;
    }
    const uint32_t address = kPageAddress + slot * 8;
    HAL_FLASH_Unlock();
    // Fails (and clears the flags) if an earlier operation left an error; the next save retries.
    bool ok = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE) == HAL_OK;
    if (ok) {
        SET_BIT(FLASH->CR, FLASH_CR_PG);
        *reinterpret_cast<volatile uint32_t*>(address) = static_cast<uint32_t>(value);
        __ISB();  // the two halves must be written in order
        *reinterpret_cast<volatile uint32_t*>(address + 4) = static_cast<uint32_t>(value >> 32);
        ok = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE) == HAL_OK;
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
    }
    HAL_FLASH_Lock();
    return ok;
}

const Flash kInternal = {reinterpret_cast<const volatile uint64_t*>(kPageAddress), erase_page,
                         program_slot};

}  // namespace

const Flash& internal() { return kInternal; }

}  // namespace flash_store
