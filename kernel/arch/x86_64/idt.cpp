#include "idt.hpp"

#include <stddef.h>
#include <stdint.h>

#include "regs.hpp"

namespace {

struct __attribute__((packed)) IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

struct __attribute__((packed)) IdtDescriptor {
    uint16_t size;
    uint64_t base;
};

static IdtEntry g_idt[256];

extern "C" void isr_stub_default();

constexpr uint8_t IDT_FLAG_INTGATE = 0x8E;

void set_entry(IdtEntry& e, void (*handler)()) {
    uint64_t addr = reinterpret_cast<uint64_t>(handler);
    e.offset_low = addr & 0xFFFF;
    e.selector = 0x08;  // kernel code segment
    e.ist = 0;
    e.type_attr = IDT_FLAG_INTGATE;
    e.offset_mid = (addr >> 16) & 0xFFFF;
    e.offset_high = (addr >> 32) & 0xFFFFFFFF;
    e.zero = 0;
}

}  // namespace

extern "C" void load_idt(IdtDescriptor* desc);

void init_idt() {
    for (size_t i = 0; i < 256; ++i) {
        set_entry(g_idt[i], isr_stub_default);
    }

    IdtDescriptor idtr{
        .size = static_cast<uint16_t>(sizeof(g_idt) - 1),
        .base = reinterpret_cast<uint64_t>(&g_idt[0]),
    };
    load_idt(&idtr);
}
