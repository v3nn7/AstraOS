// Simple SMP bring-up: MADT scan (minimal), INIT/SIPI, per-core stacks.
#include "smp.hpp"

#include "lapic.hpp"
#include "../x86_64/idt.hpp"
#include "../../util/logger.hpp"
#include "../../drivers/usb/usb_core.hpp"

extern "C" void* kmemalign(size_t alignment, size_t size);
extern "C" uintptr_t virt_to_phys(const void* p);

namespace smp {

namespace {

struct Core {
    uint32_t apic_id;
    uint8_t started;
    void* stack_top;
};

constexpr uint32_t kMaxCores = 16;
static Core g_cores[kMaxCores];
static uint32_t g_core_count = 1;
static uint32_t g_bsp_apic = 0;

// Minimal MADT header and entries (only LAPIC and x2APIC not handled).
struct __attribute__((packed)) MadtHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
    uint32_t lapic_addr;
    uint32_t flags;
};

struct __attribute__((packed)) MadtEntryHeader {
    uint8_t type;
    uint8_t length;
};

struct __attribute__((packed)) MadtLapic {
    MadtEntryHeader hdr;
    uint8_t acpi_cpu_id;
    uint8_t apic_id;
    uint32_t flags;
};

// Placeholder: in real code, map ACPI and find MADT; here we just rely on BSP only if not provided.
bool parse_madt() {
#ifdef HOST_TEST
    g_core_count = 1;
    g_bsp_apic = 0;
    g_cores[0].apic_id = 0;
    g_cores[0].started = 1;
    g_cores[0].stack_top = nullptr;
    return true;
#else
    // TODO: map RSDP/SDT and locate MADT; for now, assume only BSP.
    g_core_count = 1;
    g_bsp_apic = lapic::id();
    g_cores[0].apic_id = g_bsp_apic;
    g_cores[0].started = 1;
    g_cores[0].stack_top = nullptr;
    return true;
#endif
}

extern "C" void ap_entry();

// Trampoline page (real-mode SIPI target); minimal stub sets a flag then hlt.
alignas(4096) static uint8_t g_trampoline[4096];

[[maybe_unused]] void build_trampoline(uint8_t apic_id) {
#ifndef HOST_TEST
    // Real-mode 16-bit code: cli; mov byte [flag],1; hlt; jmp $.
    g_trampoline[0] = 0xFA;  // cli
    g_trampoline[1] = 0xC6;  // mov byte [disp8], imm8
    g_trampoline[2] = 0x06;
    g_trampoline[3] = 0x00;
    g_trampoline[4] = 0x00;  // offset within page
    g_trampoline[5] = 0x01;  // value
    g_trampoline[6] = 0xF4;  // hlt
    g_trampoline[7] = 0xEB;  // jmp -2
    g_trampoline[8] = 0xFE;
    (void)apic_id;
#else
    (void)apic_id;
#endif
}

bool start_aps() {
#ifndef HOST_TEST
    for (uint32_t i = 1; i < g_core_count; ++i) {
        Core& c = g_cores[i];
        if (c.apic_id == g_bsp_apic) continue;
        c.stack_top = kmemalign(16, 8192);
        build_trampoline(static_cast<uint8_t>(c.apic_id));
        uintptr_t vec = (virt_to_phys(g_trampoline) >> 12) & 0xFF;
        // INIT IPI
        lapic::send_ipi(static_cast<uint8_t>(c.apic_id), 0x00004500);
        // Small delay
        for (volatile int d = 0; d < 100000; ++d) {
        }
        // SIPI twice
        lapic::send_ipi(static_cast<uint8_t>(c.apic_id), 0x00004600 | vec);
        for (volatile int d = 0; d < 100000; ++d) {
        }
        lapic::send_ipi(static_cast<uint8_t>(c.apic_id), 0x00004600 | vec);
    }
#endif
    return true;
}

}  // namespace

bool init() {
    klog("smp: init");
    if (!lapic::init()) {
        klog("smp: lapic init failed");
        return false;
    }
    g_bsp_apic = lapic::id();
    if (!parse_madt()) {
        klog("smp: madt parse failed");
        return false;
    }
    if (!start_aps()) {
        klog("smp: start APs failed");
        return false;
    }
    return true;
}

uint32_t core_count() { return g_core_count; }
uint32_t bsp_apic_id() { return g_bsp_apic; }

}  // namespace smp
