#include "gdt.hpp"

#include <stdint.h>

namespace {

struct __attribute__((packed)) GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct __attribute__((packed)) GdtDescriptor {
    uint16_t size;
    uint64_t base;
};

// Null, code, data.
static GdtEntry g_gdt[3];

constexpr uint8_t ACCESS_CODE = 0x9A;  // present, ring0, code, readable
constexpr uint8_t ACCESS_DATA = 0x92;  // present, ring0, data, writable
constexpr uint8_t GRAN_4K_64 = 0x20 | 0xC0;  // 64-bit + granularity + default opsize?

void set_entry(GdtEntry& e, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    e.limit_low = limit & 0xFFFF;
    e.base_low = base & 0xFFFF;
    e.base_mid = (base >> 16) & 0xFF;
    e.access = access;
    e.granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    e.base_high = (base >> 24) & 0xFF;
}

}  // namespace

extern "C" void load_gdt(GdtDescriptor* desc, uint16_t code_sel, uint16_t data_sel);

void init_gdt() {
    set_entry(g_gdt[0], 0, 0, 0, 0);
    set_entry(g_gdt[1], 0, 0xFFFFF, ACCESS_CODE, GRAN_4K_64);
    set_entry(g_gdt[2], 0, 0xFFFFF, ACCESS_DATA, GRAN_4K_64);

    GdtDescriptor gdtr{
        .size = static_cast<uint16_t>(sizeof(g_gdt) - 1),
        .base = reinterpret_cast<uint64_t>(&g_gdt[0]),
    };

    load_gdt(&gdtr, 0x08, 0x10);
}
