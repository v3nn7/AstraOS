// Simple SMP bring-up: MADT scan (minimal), INIT/SIPI, per-core stacks.
#include "smp.hpp"

#include "lapic.hpp"
#include "../x86_64/idt.hpp"
#include "../../util/logger.hpp"
#include "../../include/klog.h"
#include "../../include/acpi.h"
#include <drivers/usb/usb_core.h>
#include <drivers/serial.hpp>

extern "C" void serial_write(const char* s);
extern "C" void serial_init(void);

// #region agent log
static bool dbg_serial_ready = false;
static inline void dbg_ensure_serial() {
    if (!dbg_serial_ready) {
        serial_init();
        dbg_serial_ready = true;
    }
}

static inline void dbg_log_smp(const char* loc, const char* msg, const char* hypo, uint64_t val, const char* key, const char* runId) {
    dbg_ensure_serial();
    char buf[128];
    int p = 0;
    auto app = [&](const char* s) { while (*s && p < (int)sizeof(buf)-1) buf[p++] = *s++; };
    app("{\"sessionId\":\"debug-session\",\"runId\":\""); app(runId);
    app("\",\"hypothesisId\":\""); app(hypo);
    app("\",\"location\":\""); app(loc);
    app("\",\"message\":\""); app(msg);
    app("\",\"data\":{\""); app(key); app("\":");
    // hex
    char hex[19]; hex[0]='0'; hex[1]='x';
    for (int i=0;i<16;i++){ uint8_t n=(val>>(60-4*i))&0xF; hex[2+i]=(n<10)?('0'+n):('a'+n-10);}
    hex[18]='\0';
    app("\""); app(hex); app("\"},\"timestamp\":0}");
    buf[p]='\0';
    serial_write(buf); serial_write("\n");
}
// #endregion

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

bool parse_madt() {
#ifdef HOST_TEST
    g_core_count = 1;
    g_bsp_apic = 0;
    g_cores[0].apic_id = 0;
    g_cores[0].started = 1;
    g_cores[0].stack_top = nullptr;
    return true;
#else
    uint32_t bsp_apic_id = lapic::id();
    uint8_t lapic_count = acpi_get_lapic_count();
    
    if (lapic_count == 0) {
        klog_printf(KLOG_WARN, "smp: no LAPIC entries in MADT, using BSP only");
        g_core_count = 1;
        g_bsp_apic = bsp_apic_id;
        g_cores[0].apic_id = bsp_apic_id;
        g_cores[0].started = 1;
        g_cores[0].stack_top = nullptr;
        return true;
    }
    
    g_core_count = 0;
    g_bsp_apic = bsp_apic_id;
    
    for (uint8_t i = 0; i < lapic_count && g_core_count < kMaxCores; i++) {
        uint8_t acpi_cpu_id, apic_id;
        uint32_t flags;
        if (!acpi_get_lapic_entry(i, &acpi_cpu_id, &apic_id, &flags)) continue;
        
        if ((flags & 1) == 0) {
            klog_printf(KLOG_DEBUG, "smp: skipping disabled CPU acpi_id=%u apic_id=%u",
                        acpi_cpu_id, apic_id);
            continue;
        }
        
        g_cores[g_core_count].apic_id = apic_id;
        g_cores[g_core_count].started = (apic_id == bsp_apic_id) ? 1 : 0;
        g_cores[g_core_count].stack_top = nullptr;
        g_core_count++;
    }
    
    if (g_core_count == 0) {
        klog_printf(KLOG_WARN, "smp: no enabled CPUs found, using BSP only");
        g_core_count = 1;
        g_cores[0].apic_id = bsp_apic_id;
        g_cores[0].started = 1;
        g_cores[0].stack_top = nullptr;
    }
    
    klog_printf(KLOG_INFO, "smp: found %u CPUs (BSP apic_id=%u)", 
               g_core_count, bsp_apic_id);
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
    uint32_t started_count = 0;
    for (uint32_t i = 0; i < g_core_count; ++i) {
        Core& c = g_cores[i];
        if (c.apic_id == g_bsp_apic || c.started) continue;
        
        klog_printf(KLOG_DEBUG, "smp: starting AP apic_id=%u", c.apic_id);
        
        c.stack_top = kmemalign(16, 8192);
        if (!c.stack_top) {
            klog_printf(KLOG_WARN, "smp: failed to allocate stack for AP apic_id=%u", c.apic_id);
            continue;
        }
        
        build_trampoline(static_cast<uint8_t>(c.apic_id));
        uintptr_t vec = (virt_to_phys(g_trampoline) >> 12) & 0xFF;
        
        if (vec == 0 || vec > 0xFF) {
            klog_printf(KLOG_ERROR, "smp: invalid trampoline vector 0x%llx for AP apic_id=%u", 
                       (unsigned long long)vec, c.apic_id);
            continue;
        }
        
        klog_printf(KLOG_DEBUG, "smp: sending INIT IPI to apic_id=%u", c.apic_id);
        lapic::send_ipi(static_cast<uint8_t>(c.apic_id), 0x00004500);
        
        for (volatile int d = 0; d < 200000; ++d) {
        }
        
        klog_printf(KLOG_DEBUG, "smp: sending SIPI to apic_id=%u vector=0x%02x", c.apic_id, (unsigned)vec);
        lapic::send_ipi(static_cast<uint8_t>(c.apic_id), 0x00004600 | vec);
        
        for (volatile int d = 0; d < 200000; ++d) {
        }
        
        lapic::send_ipi(static_cast<uint8_t>(c.apic_id), 0x00004600 | vec);
        
        for (volatile int d = 0; d < 200000; ++d) {
        }
        
        started_count++;
    }
    
    if (started_count > 0) {
        klog_printf(KLOG_INFO, "smp: initiated startup for %u AP cores", started_count);
    }
#endif
    return true;
}

}  // namespace

bool init() {
    dbg_log_smp("smp.cpp:init", "smp_init_enter", "H4", 0, "stage", "run-pre");
    klog_printf(KLOG_INFO, "smp: initializing");
    
    if (!lapic::init()) {
        dbg_log_smp("smp.cpp:init", "smp_lapic_fail", "H4", 0, "stage", "run-pre");
        klog_printf(KLOG_ERROR, "smp: lapic init failed");
        return false;
    }
    
    g_bsp_apic = lapic::id();
    dbg_log_smp("smp.cpp:init", "smp_after_lapic", "H4", g_bsp_apic, "lapic_id", "run-pre");
    klog_printf(KLOG_INFO, "smp: BSP apic_id=%u", g_bsp_apic);
    
    if (!parse_madt()) {
        dbg_log_smp("smp.cpp:init", "smp_madt_fail", "H4", 0, "stage", "run-pre");
        klog_printf(KLOG_ERROR, "smp: madt parse failed");
        return false;
    }
    
    dbg_log_smp("smp.cpp:init", "smp_after_madt", "H4", g_core_count, "cores", "run-pre");
    
    if (g_core_count > 1) {
        klog_printf(KLOG_INFO, "smp: starting %u AP cores", g_core_count - 1);
        if (!start_aps()) {
            dbg_log_smp("smp.cpp:init", "smp_start_aps_fail", "H4", 0, "stage", "run-pre");
            klog_printf(KLOG_WARN, "smp: start APs failed, continuing with BSP only");
        } else {
            klog_printf(KLOG_INFO, "smp: AP startup initiated");
        }
    } else {
        klog_printf(KLOG_INFO, "smp: single core (BSP only)");
    }
    
    dbg_log_smp("smp.cpp:init", "smp_init_done", "H4", g_core_count, "cores", "run-pre");
    klog_printf(KLOG_INFO, "smp: initialization complete, %u cores", g_core_count);
    return true;
}

uint32_t core_count() { return g_core_count; }
uint32_t bsp_apic_id() { return g_bsp_apic; }

}  // namespace smp
