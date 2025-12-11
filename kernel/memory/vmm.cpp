#include "vmm.hpp"
#include "vmm_internal.hpp"
#include <drivers/serial.hpp>

extern "C" void serial_write(const char* s);
extern "C" void serial_init(void);

extern "C" void load_cr3(uint64_t);
extern "C" uint64_t read_cr3();
extern "C" uint64_t read_cr0();
extern "C" uint64_t read_cr4();
extern "C" void write_cr0(uint64_t);
extern "C" void write_cr4(uint64_t);

/* ------------------------------------------------------------------------- */
/*      Prosty, ale rozszerzalny allocator na page tables                    */
/* ------------------------------------------------------------------------- */
static uint8_t pt_pool[4096 * 256] __attribute__((aligned(4096)));
static size_t  pt_offset = 0;

// #region agent log
static bool dbg_serial_ready = false;
static inline void dbg_ensure_serial() {
    if (!dbg_serial_ready) {
        serial_init();
        dbg_serial_ready = true;
    }
}

static inline void dbg_hex16(char* out, uint64_t v) {
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        uint8_t nib = (v >> (60 - 4 * i)) & 0xF;
        out[2 + i] = (nib < 10) ? ('0' + nib) : ('a' + nib - 10);
    }
    out[18] = '\0';
}

static inline void dbg_log_vmm(const char* location, const char* message, const char* hypo,
                               uint64_t key_val, const char* key_name, const char* runId) {
    dbg_ensure_serial();
    char hex[19]; dbg_hex16(hex, key_val);
    char buf[256];
    int p = 0;
    auto append = [&](const char* s) {
        while (*s && p < (int)(sizeof(buf) - 1)) buf[p++] = *s++;
    };
    append("{\"sessionId\":\"debug-session\",\"runId\":\"");
    append(runId);
    append("\",\"hypothesisId\":\"");
    append(hypo);
    append("\",\"location\":\"");
    append(location);
    append("\",\"message\":\"");
    append(message);
    append("\",\"data\":{\"");
    append(key_name);
    append("\":\"");
    append(hex);
    append("\"},\"timestamp\":0}");
    buf[p] = '\0';
    serial_write(buf);
    serial_write("\n");
}
// #endregion

static VMM::PageTable* alloc_pt() {
    if (pt_offset + sizeof(VMM::PageTable) > sizeof(pt_pool))
        return nullptr; // powinieneś zrobić panic()

    auto* t = reinterpret_cast<VMM::PageTable*>(pt_pool + pt_offset);
    pt_offset += sizeof(VMM::PageTable);

    for (auto &e : t->entry) e = 0;
    return t;
}

/* global PML4 */
namespace VMM {
    PageTable* pml4 = nullptr;
}

/* ------------------------------------------------------------------------- */
/*      Helper: get or create paging levels                                  */
/* ------------------------------------------------------------------------- */
static VMM::PageTable* get_or_create(VMM::PageTable* table, uint16_t idx, uint64_t flags) {
    if (!(table->entry[idx] & VMM::PRESENT)) {
        VMM::PageTable* new_tbl = alloc_pt();
        table->entry[idx] = (uint64_t)new_tbl | VMM::PRESENT | VMM::WRITABLE | (flags & 0xFFF);
    }
    return (VMM::PageTable*)(table->entry[idx] & ~0xFFF);
}

/* ------------------------------------------------------------------------- */
/*      Mapowanie stron                                                      */
/* ------------------------------------------------------------------------- */
void VMM::map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint16_t i_pml4 = (virt >> 39) & 0x1FF;
    uint16_t i_pdpt = (virt >> 30) & 0x1FF;
    uint16_t i_pd   = (virt >> 21) & 0x1FF;
    uint16_t i_pt   = (virt >> 12) & 0x1FF;

    auto* pdpt = get_or_create(pml4, i_pml4, flags);
    auto* pd   = get_or_create(pdpt, i_pdpt, flags);

    if (flags & HUGE_1GB) {
        pd->entry[i_pd] = (phys & ~0x3FFFFFFF) | (flags | PRESENT | WRITABLE);
        return;
    }

    auto* ptbl = get_or_create(pd, i_pd, flags);

    if (flags & HUGE_2MB) {
        pd->entry[i_pd] = (phys & ~0x1FFFFF) | (flags | PRESENT | WRITABLE | (1ull << 7));
        return;
    }

    ptbl->entry[i_pt] = (phys & ~0xFFF) | (flags | PRESENT);
}

/* ------------------------------------------------------------------------- */
/*     Unmap                                                                 */
/* ------------------------------------------------------------------------- */
void VMM::unmap(uint64_t virt) {
    uint16_t i_pml4 = (virt >> 39) & 0x1FF;
    uint16_t i_pdpt = (virt >> 30) & 0x1FF;
    uint16_t i_pd   = (virt >> 21) & 0x1FF;
    uint16_t i_pt   = (virt >> 12) & 0x1FF;

    auto* pdpt = (VMM::PageTable*)(pml4->entry[i_pml4] & ~0xFFF);
    if (!pdpt) return;
    auto* pd = (VMM::PageTable*)(pdpt->entry[i_pdpt] & ~0xFFF);
    if (!pd) return;

    if (pd->entry[i_pd] & (1ull << 7)) {  // 2MB page
        pd->entry[i_pd] = 0;
        return;
    }

    auto* ptbl = (VMM::PageTable*)(pd->entry[i_pd] & ~0xFFF);
    if (!ptbl) return;

    ptbl->entry[i_pt] = 0;
}

/* ------------------------------------------------------------------------- */
/*     Translate VA → PA                                                    */
/* ------------------------------------------------------------------------- */
uint64_t VMM::translate(uint64_t virt) {
    uint16_t i_pml4 = (virt >> 39) & 0x1FF;
    uint16_t i_pdpt = (virt >> 30) & 0x1FF;
    uint16_t i_pd   = (virt >> 21) & 0x1FF;
    uint16_t i_pt   = (virt >> 12) & 0x1FF;

    auto* pdpt = (PageTable*)(pml4->entry[i_pml4] & ~0xFFF);
    if (!pdpt) return 0;

    auto* pd = (PageTable*)(pdpt->entry[i_pdpt] & ~0xFFF);
    if (!pd) return 0;

    if (pd->entry[i_pd] & (1ull << 7)) {
        uint64_t base = pd->entry[i_pd] & ~0x1FFFFF;
        return base + (virt & 0x1FFFFF);
    }

    auto* ptbl = (PageTable*)(pd->entry[i_pd] & ~0xFFF);
    if (!ptbl) return 0;

    if (!(ptbl->entry[i_pt] & PRESENT)) return 0;

    uint64_t base = ptbl->entry[i_pt] & ~0xFFF;
    return base + (virt & 0xFFF);
}

/* ------------------------------------------------------------------------- */
/*     MMIO mapping (uncached)                                               */
/* ------------------------------------------------------------------------- */
uintptr_t VMM::map_mmio(uintptr_t phys, size_t size) {
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uintptr_t virt = alloc_virt_region(size);
    dbg_log_vmm("vmm.cpp:map_mmio:enter", "map_mmio_enter", "H3", phys, "phys", "run-pre");
    dbg_log_vmm("vmm.cpp:map_mmio:pml4", "map_mmio_pml4", "H1", (uint64_t)pml4, "pml4_ptr", "run-pre");

    for (size_t off = 0; off < size; off += PAGE_SIZE) {
        map(virt + off, phys + off, WRITABLE | NO_CACHE | WRITE_THROUGH | PRESENT);
    }

    return virt;
}

/* ------------------------------------------------------------------------- */
/*     Virtual region allocator (monotonic)                                  */
/* ------------------------------------------------------------------------- */
static uintptr_t virt_base = 0xffff900000000000ull;

uintptr_t VMM::alloc_virt_region(size_t size) {
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uintptr_t ret = virt_base;
    virt_base += size;
    return ret;
}

/* ------------------------------------------------------------------------- */
/*     Initialization                                                        */
/* ------------------------------------------------------------------------- */
void VMM::init() {
    pml4 = alloc_pt();
    dbg_log_vmm("vmm.cpp:VMM::init:alloc", "vmm_init_pml4", "H2", (uint64_t)pml4, "pml4_ptr", "run-pre");

    /* enable PAE */
    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 5);
    write_cr4(cr4);
    /* load CR3 */
    load_cr3((uint64_t)pml4);

    /* enable paging */
    uint64_t cr0 = read_cr0();
    cr0 |= (1 << 31);
    write_cr0(cr0);
    dbg_log_vmm("vmm.cpp:VMM::init:cr0", "vmm_init_after_cr0", "H2", cr0, "cr0", "run-pre");
}

/* C wrapper API */
extern "C" {
    void vmm_init() { VMM::init(); }
    void vmm_map(uintptr_t v, uintptr_t p, uint32_t f) { VMM::map(v,p,f); }
    void vmm_unmap(uintptr_t v) { VMM::unmap(v); }
    uintptr_t vmm_translate(uintptr_t v) { return VMM::translate(v); }
    uintptr_t vmm_map_mmio(uintptr_t p, size_t s) { return VMM::map_mmio(p,s); }
    uintptr_t vmm_alloc_region(size_t s) { return VMM::alloc_virt_region(s); }
}
