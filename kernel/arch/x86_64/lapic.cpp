// Local APIC accessors and simple timer setup.
#include "lapic.hpp"

extern "C" uint64_t rdmsr(uint32_t msr) {
#ifdef HOST_TEST
    (void)msr;
    return 0;
#else
    uint32_t lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#endif
}

extern "C" void wrmsr(uint32_t msr, uint64_t val) {
#ifdef HOST_TEST
    (void)msr;
    (void)val;
#else
    uint32_t lo = static_cast<uint32_t>(val & 0xFFFFFFFFu);
    uint32_t hi = static_cast<uint32_t>((val >> 32) & 0xFFFFFFFFu);
    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
#endif
}
extern "C" uint32_t mmio_read32(uintptr_t addr);
extern "C" void mmio_write32(uintptr_t addr, uint32_t val);

namespace lapic {

namespace {

uintptr_t g_base = 0;

constexpr uint32_t kMsrApicBase = 0x1B;
constexpr uint32_t kApicEnable = 1 << 11;

constexpr uint32_t kRegId = 0x020;
constexpr uint32_t kRegSvr = 0x0F0;
constexpr uint32_t kRegIcrLow = 0x300;
constexpr uint32_t kRegIcrHigh = 0x310;
constexpr uint32_t kRegTimerLvt = 0x320;
constexpr uint32_t kRegTimerInit = 0x380;
constexpr uint32_t kRegTimerDiv = 0x3E0;

}  // namespace

bool init() {
#ifdef HOST_TEST
    return true;
#else
    uint64_t apic_base = rdmsr(kMsrApicBase);
    apic_base |= kApicEnable;
    wrmsr(kMsrApicBase, apic_base);
    g_base = static_cast<uintptr_t>(apic_base & 0xFFFFF000);
    // Set spurious interrupt vector enable.
    write(kRegSvr, read(kRegSvr) | 0x100 | 0xFF);
    return true;
#endif
}

void write(uint32_t reg, uint32_t val) {
#ifdef HOST_TEST
    (void)reg;
    (void)val;
#else
    mmio_write32(g_base + reg, val);
#endif
}

uint32_t read(uint32_t reg) {
#ifdef HOST_TEST
    (void)reg;
    return 0;
#else
    return mmio_read32(g_base + reg);
#endif
}

uint32_t id() {
    return read(kRegId) >> 24;
}

void send_ipi(uint8_t apic_id, uint32_t icr_low) {
#ifndef HOST_TEST
    write(kRegIcrHigh, static_cast<uint32_t>(apic_id) << 24);
    write(kRegIcrLow, icr_low);
#else
    (void)apic_id;
    (void)icr_low;
#endif
}

void timer_init(uint8_t vector, uint32_t divide, uint32_t initial) {
#ifndef HOST_TEST
    write(kRegTimerDiv, divide);
    write(kRegTimerLvt, vector);
    write(kRegTimerInit, initial);
#else
    (void)vector;
    (void)divide;
    (void)initial;
#endif
}

}  // namespace lapic
