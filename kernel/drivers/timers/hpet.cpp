#include "hpet.hpp"
#include "acpi.h"
#include "vmm.h"

volatile uint64_t* HPET::hpet_base = nullptr;
uint64_t HPET::hpet_frequency = 0;

static inline uint64_t hpet_read(uint32_t reg) {
    return HPET::hpet_base[reg / 8];
}

static inline void hpet_write(uint32_t reg, uint64_t val) {
    HPET::hpet_base[reg / 8] = val;
}

// HPET registers (offsets in bytes)
#define HPET_GENERAL_CAP     0x000
#define HPET_GENERAL_CONFIG  0x010
#define HPET_MAIN_COUNTER    0x0F0

void HPET::init() {
    // 1) Pobierz fizyczny adres HPET z ACPI
    uint64_t phys = acpi_get_hpet_address();

    // 2) Zmapuj HPET jako MMIO
    hpet_base = (volatile uint64_t*)vmm_map_mmio(phys, 0x1000);

    // 3) Pobierz częstotliwość HPET
    uint64_t caps = hpet_read(HPET_GENERAL_CAP);

    uint32_t period_fs = (uint32_t)(caps >> 32); // femtoseconds per tick
    hpet_frequency = 1'000'000'000'000'000ULL / period_fs; // Hz

    // 4) Wyzeruj counter
    hpet_write(HPET_MAIN_COUNTER, 0);

    // 5) Włącz HPET (bit 0)
    uint64_t cfg = hpet_read(HPET_GENERAL_CONFIG);
    cfg |= 1;
    hpet_write(HPET_GENERAL_CONFIG, cfg);
}

uint64_t HPET::counter() {
    return hpet_read(HPET_MAIN_COUNTER);
}

void HPET::sleep_us(uint64_t us) {
    uint64_t start = counter();
    uint64_t target = start + (hpet_frequency * us) / 1'000'000ULL;

    while (counter() < target) {
        asm volatile("pause");
    }
}

void HPET::sleep_ms(uint64_t ms) {
    sleep_us(ms * 1000);
}
