#include "paging.hpp"

extern "C" void load_cr3(uint64_t);
extern "C" uint64_t read_cr0();
extern "C" uint64_t read_cr4();
extern "C" void write_cr0(uint64_t);
extern "C" void write_cr4(uint64_t);

/* ------------------------------------------------------------------------- */
/*               Prosty allocator na page tables                              */
/* ------------------------------------------------------------------------- */

static uint8_t pt_pool[4096 * 256] __attribute__((aligned(4096)));
static size_t  pt_offset = 0;

static Paging::PageTable* alloc_pt() {
    if (pt_offset + sizeof(Paging::PageTable) > sizeof(pt_pool))
        return nullptr;

    auto* t = reinterpret_cast<Paging::PageTable*>(pt_pool + pt_offset);
    pt_offset += sizeof(Paging::PageTable);

    for (auto &e : t->entry) e = 0;

    return t;
}

namespace Paging {
    PageTable* pml4 = nullptr;
}

/* ------------------------------------------------------------------------- */
/*                           Helpery paging                                   */
/* ------------------------------------------------------------------------- */

static Paging::PageTable* get_or_create(Paging::PageTable* tbl, uint16_t idx) {
    if (!(tbl->entry[idx] & Paging::PRESENT)) {
        auto* new_tbl = alloc_pt();
        tbl->entry[idx] = (uint64_t)new_tbl | Paging::PRESENT | Paging::WRITABLE;
    }
    return (Paging::PageTable*)(tbl->entry[idx] & ~0xFFF);
}

/* ------------------------------------------------------------------------- */
/*                           Mapowanie stron                                  */
/* ------------------------------------------------------------------------- */

void Paging::map(uint64_t virt, uint64_t phys, uint64_t flags) {

    uint16_t pml4_i = (virt >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt >> 30) & 0x1FF;
    uint16_t pd_i   = (virt >> 21) & 0x1FF;
    uint16_t pt_i   = (virt >> 12) & 0x1FF;

    auto* pdpt = get_or_create(pml4, pml4_i);
    auto* pd   = get_or_create(pdpt, pdpt_i);

    /* ---- 1GB huge page ---- */
    if (flags & HUGE_1GB) {
        pd->entry[pd_i] = (phys & ~0x3FFFFFFF) | flags | PRESENT | WRITABLE;
        return;
    }

    /* ---- 2MB huge page ---- */
    if (flags & HUGE_2MB) {
        pd->entry[pd_i] = (phys & ~0x1FFFFF) | flags | PRESENT | WRITABLE | (1ull << 7);
        return;
    }

    auto* pt = get_or_create(pd, pd_i);
    pt->entry[pt_i] = (phys & ~0xFFF) | flags | PRESENT;
}

/* ------------------------------------------------------------------------- */
/*                            Unmap                                           */
/* ------------------------------------------------------------------------- */

void Paging::unmap(uint64_t virt) {

    uint16_t pml4_i = (virt >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt >> 30) & 0x1FF;
    uint16_t pd_i   = (virt >> 21) & 0x1FF;
    uint16_t pt_i   = (virt >> 12) & 0x1FF;

    auto* pdpt = (PageTable*)(pml4->entry[pml4_i] & ~0xFFF);
    if (!pdpt) return;

    auto* pd = (PageTable*)(pdpt->entry[pdpt_i] & ~0xFFF);
    if (!pd) return;

    /* hugepage 2MB */
    if (pd->entry[pd_i] & (1ull << 7)) {
        pd->entry[pd_i] = 0;
        return;
    }

    auto* pt = (PageTable*)(pd->entry[pd_i] & ~0xFFF);
    if (!pt) return;

    pt->entry[pt_i] = 0;
}

/* ------------------------------------------------------------------------- */
/*                      Translate VA → PA                                    */
/* ------------------------------------------------------------------------- */

uint64_t Paging::translate(uint64_t virt) {

    uint16_t pml4_i = (virt >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt >> 30) & 0x1FF;
    uint16_t pd_i   = (virt >> 21) & 0x1FF;
    uint16_t pt_i   = (virt >> 12) & 0x1FF;

    auto* pdpt = (PageTable*)(pml4->entry[pml4_i] & ~0xFFF);
    if (!pdpt) return 0;

    auto* pd = (PageTable*)(pdpt->entry[pdpt_i] & ~0xFFF);
    if (!pd) return 0;

    /* hugepage 2MB */
    if (pd->entry[pd_i] & (1ull << 7)) {
        uint64_t base = pd->entry[pd_i] & ~0x1FFFFF;
        return base + (virt & 0x1FFFFF);
    }

    auto* pt = (PageTable*)(pd->entry[pd_i] & ~0xFFF);
    if (!pt) return 0;

    if (!(pt->entry[pt_i] & PRESENT))
        return 0;

    uint64_t base = pt->entry[pt_i] & ~0xFFF;
    return base + (virt & 0xFFF);
}

/* ------------------------------------------------------------------------- */
/*                           MMIO mapping                                     */
/* ------------------------------------------------------------------------- */

static uint64_t virt_next = 0xffff900000000000ull;

uint64_t Paging::alloc_virt_region(size_t size) {
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t base = virt_next;
    virt_next += size;
    return base;
}

uint64_t Paging::map_mmio(uint64_t phys, size_t size) {
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint64_t virt = alloc_virt_region(size);

    for (size_t off = 0; off < size; off += PAGE_SIZE) {
        map(virt + off, phys + off, PRESENT | WRITABLE | NO_CACHE);
    }
    return virt;
}

/* ------------------------------------------------------------------------- */
/*                        Init paging                                        */
/* ------------------------------------------------------------------------- */

void Paging::init() {

    pml4 = alloc_pt();

    /* Enable PAE */
    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 5);
    write_cr4(cr4);

    /* Load CR3 */
    load_cr3((uint64_t)pml4);

    /* Enable paging */
    uint64_t cr0 = read_cr0();
    cr0 |= (1 << 31);
    write_cr0(cr0);
}
