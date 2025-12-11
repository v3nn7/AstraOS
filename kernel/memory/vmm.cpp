#include "vmm.hpp"
#include "vmm_internal.hpp"
#include <drivers/serial.hpp>

extern "C" void serial_write(const char* s);
extern "C" void serial_init(void);
extern "C" uint64_t pmm_hhdm_offset;

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

// Forward declaration for the early helper.
static VMM::PageTable* alloc_pt();

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

[[maybe_unused]] static inline void dbg_log_vmm(const char* location, const char* message, const char* hypo,
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

namespace {

constexpr uint64_t PAGE_4K = 0x1000ull;
constexpr uint64_t PAGE_2M = 0x200000ull;
constexpr uint64_t PAGE_1G = 0x40000000ull;
constexpr uint64_t IDENTITY_MAX = 0x100000000ull; // cap identity map to first 4 GiB

struct EfiMemDesc {
    uint32_t type;
    uint32_t pad;
    uint64_t phys;
    uint64_t virt;
    uint64_t pages;
    uint64_t attrs;
};

static VMM::PageTable* ensure_pml4() {
    if (!VMM::pml4) {
        VMM::pml4 = alloc_pt();
    }
    return VMM::pml4;
}

static void map_range(uintptr_t virt, uintptr_t phys, size_t size, uint64_t flags) {
    size = (size + VMM::PAGE_SIZE - 1) & ~(VMM::PAGE_SIZE - 1);
    while (size > 0) {
        if ((virt % PAGE_1G == 0) && (phys % PAGE_1G == 0) && size >= PAGE_1G) {
            VMM::map(virt, phys, flags | VMM::HUGE_1GB);
            virt += PAGE_1G;
            phys += PAGE_1G;
            size -= PAGE_1G;
            continue;
        }
        if ((virt % PAGE_2M == 0) && (phys % PAGE_2M == 0) && size >= PAGE_2M) {
            VMM::map(virt, phys, flags | VMM::HUGE_2MB);
            virt += PAGE_2M;
            phys += PAGE_2M;
            size -= PAGE_2M;
            continue;
        }
        VMM::map(virt, phys, flags);
        virt += PAGE_4K;
        phys += PAGE_4K;
        size -= PAGE_4K;
    }
}

}  // namespace

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
    if (!table) return nullptr;
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
    auto* root = ensure_pml4();
    if (!root) return;

    uint16_t i_pml4 = (virt >> 39) & 0x1FF;
    uint16_t i_pdpt = (virt >> 30) & 0x1FF;
    uint16_t i_pd   = (virt >> 21) & 0x1FF;
    uint16_t i_pt   = (virt >> 12) & 0x1FF;

    auto* pdpt = get_or_create(root, i_pml4, flags);
    if (!pdpt) return;

    auto* pd   = get_or_create(pdpt, i_pdpt, flags);
    if (!pd) return;

    if (flags & VMM::HUGE_1GB) {
        pd->entry[i_pd] = (phys & ~0x3FFFFFFF) | (flags | VMM::PRESENT | VMM::WRITABLE);
        return;
    }

    if (flags & VMM::HUGE_2MB) {
        pd->entry[i_pd] = (phys & ~0x1FFFFF) | (flags | VMM::PRESENT | VMM::WRITABLE | (1ull << 7));
        return;
    }

    auto* ptbl = get_or_create(pd, i_pd, flags);
    if (!ptbl) return;

    ptbl->entry[i_pt] = (phys & ~0xFFF) | (flags | VMM::PRESENT);
}

/* ------------------------------------------------------------------------- */
/*     Unmap                                                                 */
/* ------------------------------------------------------------------------- */
void VMM::unmap(uint64_t virt) {
    if (!pml4) return;

    uint16_t i_pml4 = (virt >> 39) & 0x1FF;
    uint16_t i_pdpt = (virt >> 30) & 0x1FF;
    uint16_t i_pd   = (virt >> 21) & 0x1FF;
    uint16_t i_pt   = (virt >> 12) & 0x1FF;

    uint64_t pml4e = pml4->entry[i_pml4];
    if (!(pml4e & VMM::PRESENT)) return;

    auto* pdpt = (VMM::PageTable*)(pml4e & ~0xFFF);
    uint64_t pdpte = pdpt->entry[i_pdpt];
    if (!(pdpte & VMM::PRESENT)) return;

    auto* pd = (VMM::PageTable*)(pdpte & ~0xFFF);
    uint64_t pde = pd->entry[i_pd];
    if (!(pde & VMM::PRESENT)) return;

    if (pde & (1ull << 7)) {  // 2MB page
        pd->entry[i_pd] = 0;
        __asm__ __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
        return;
    }

    auto* ptbl = (VMM::PageTable*)(pde & ~0xFFF);
    uint64_t pte = ptbl->entry[i_pt];
    if (!(pte & VMM::PRESENT)) return;

    ptbl->entry[i_pt] = 0;
    __asm__ __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
}

/* ------------------------------------------------------------------------- */
/*     Translate VA → PA                                                    */
/* ------------------------------------------------------------------------- */
uint64_t VMM::translate(uint64_t virt) {
    if (!pml4) return 0;

    uint16_t i_pml4 = (virt >> 39) & 0x1FF;
    uint16_t i_pdpt = (virt >> 30) & 0x1FF;
    uint16_t i_pd   = (virt >> 21) & 0x1FF;
    uint16_t i_pt   = (virt >> 12) & 0x1FF;

    uint64_t pml4e = pml4->entry[i_pml4];
    if (!(pml4e & VMM::PRESENT)) return 0;
    auto* pdpt = (PageTable*)(pml4e & ~0xFFF);

    uint64_t pdpte = pdpt->entry[i_pdpt];
    if (!(pdpte & VMM::PRESENT)) return 0;
    auto* pd = (PageTable*)(pdpte & ~0xFFF);

    uint64_t pde = pd->entry[i_pd];
    if (!(pde & VMM::PRESENT)) return 0;

    if (pde & (1ull << 7)) {
        uint64_t base = pde & ~0x1FFFFF;
        return base + (virt & 0x1FFFFF);
    }

    auto* ptbl = (PageTable*)(pde & ~0xFFF);

    uint64_t pte = ptbl->entry[i_pt];
    if (!(pte & VMM::PRESENT)) return 0;

    uint64_t base = pte & ~0xFFF;
    return base + (virt & 0xFFF);
}

/* ------------------------------------------------------------------------- */
/*     MMIO mapping (uncached)                                               */
/* ------------------------------------------------------------------------- */
uintptr_t VMM::map_mmio(uintptr_t phys, size_t size) {
    size = (size + VMM::PAGE_SIZE - 1) & ~(VMM::PAGE_SIZE - 1);

    /* Fallback: if paging not initialized yet, rely on firmware identity map. */
    if (pml4 == nullptr) {
        return phys;
    }

    uintptr_t virt = alloc_virt_region(size);

    for (size_t off = 0; off < size; off += VMM::PAGE_SIZE) {
        map(virt + off, phys + off, VMM::WRITABLE | VMM::NO_CACHE | VMM::WRITE_THROUGH | VMM::PRESENT);
        __asm__ __volatile__("invlpg (%0)" ::"r"(virt + off) : "memory");
    }

    return virt;
}

/* ------------------------------------------------------------------------- */
/*     Virtual region allocator (monotonic)                                  */
/* ------------------------------------------------------------------------- */
static uintptr_t virt_base = 0xffff900000000000ull;

uintptr_t VMM::alloc_virt_region(size_t size) {
    size = (size + VMM::PAGE_SIZE - 1) & ~(VMM::PAGE_SIZE - 1);
    uintptr_t ret = virt_base;
    virt_base += size;
    return ret;
}

/* ------------------------------------------------------------------------- */
/*     Initialization                                                        */
/* ------------------------------------------------------------------------- */
void VMM::init(uintptr_t mem_map, size_t map_size, size_t desc_size) {
    ensure_pml4();
    const bool have_map = (mem_map != 0) && (map_size > 0) && (desc_size > 0);

    if (have_map) {
        for (size_t off = 0; off < map_size; off += desc_size) {
            const auto* d = reinterpret_cast<const EfiMemDesc*>(mem_map + off);

            // Identity map: full range for non-reserved types; for reserved/MMIO (type 0) map only low 4 GiB.
            const uint64_t bytes = d->pages * VMM::PAGE_SIZE;
            uint64_t id_len = bytes;
            if (d->type == 0) {
                if (d->phys >= IDENTITY_MAX) {
                    id_len = 0;
                } else {
                    uint64_t max_len = IDENTITY_MAX - d->phys;
                    id_len = (bytes < max_len) ? bytes : max_len;
                }
            }
            if (id_len) {
                map_range(d->phys, d->phys, id_len, VMM::WRITABLE | VMM::PRESENT);
            }

            // High-half direct map only for usable memory to avoid mapping giant reserved windows.
            if (d->type == 7 && pmm_hhdm_offset != 0) {
                map_range(pmm_hhdm_offset + d->phys, d->phys, bytes, VMM::WRITABLE | VMM::PRESENT);
            }
        }
    } else {
        // Fallback: identity + HHDM for the first 1 GiB so the kernel and boot stack stay reachable.
        const uint64_t bootstrap_window = PAGE_1G;
        map_range(0, 0, bootstrap_window, VMM::WRITABLE | VMM::PRESENT);
        if (pmm_hhdm_offset != 0) {
            map_range(pmm_hhdm_offset, 0, bootstrap_window, VMM::WRITABLE | VMM::PRESENT);
        }
    }

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
}

/* C wrapper API */
extern "C" {
    void vmm_init() { VMM::init(0, 0, 0); }
    void vmm_init_with_map(uintptr_t m, size_t s, size_t d) { VMM::init(m, s, d); }
    void vmm_map(uintptr_t v, uintptr_t p, uint32_t f) { VMM::map(v,p,f); }
    void vmm_unmap(uintptr_t v) { VMM::unmap(v); }
    uintptr_t vmm_translate(uintptr_t v) { return VMM::translate(v); }
    uintptr_t vmm_map_mmio(uintptr_t p, size_t s) { return VMM::map_mmio(p,s); }
    uintptr_t vmm_alloc_region(size_t s) { return VMM::alloc_virt_region(s); }
}