#include "types.h"
#include "memory.h"
#include "interrupts.h"

#define PML4_INDEX(x)   (((x) >> 39) & 0x1FF)
#define PDPT_INDEX(x)   (((x) >> 30) & 0x1FF)
#define PD_INDEX(x)     (((x) >> 21) & 0x1FF)
#define PT_INDEX(x)     (((x) >> 12) & 0x1FF)

static uint64_t pml4_table[512] ALIGNED(PAGE_SIZE);
static uint64_t pdpt_low[512] ALIGNED(PAGE_SIZE);
static uint64_t pd_low[512] ALIGNED(PAGE_SIZE);
static uint64_t pdpt_high[512] ALIGNED(PAGE_SIZE);
static uint64_t pd_high[512] ALIGNED(PAGE_SIZE);

#define TABLE_POOL_SIZE 32
static uint64_t table_pool[TABLE_POOL_SIZE][512] ALIGNED(PAGE_SIZE);
static size_t table_pool_used = 0;

static void *memset_local(void *dst, int c, size_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

static inline phys_addr_t phys_of(void *ptr) {
    return (phys_addr_t)((uint64_t)ptr - KERNEL_BASE);
}

static inline uint64_t make_entry(phys_addr_t phys, uint64_t flags) {
    return (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

static uint64_t *alloc_table(void) {
    if (table_pool_used >= TABLE_POOL_SIZE) return 0;
    uint64_t *tbl = table_pool[table_pool_used++];
    memset_local(tbl, 0, PAGE_SIZE);
    return tbl;
}

static uint64_t *get_table(uint64_t entry) {
    phys_addr_t phys = (phys_addr_t)(entry & ~0xFFFULL);
    return (uint64_t *)(phys + KERNEL_BASE);
}

int map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    uint16_t pml4_i = PML4_INDEX(virt);
    uint16_t pdpt_i = PDPT_INDEX(virt);
    uint16_t pd_i   = PD_INDEX(virt);
    uint16_t pt_i   = PT_INDEX(virt);

    if (!(pml4_table[pml4_i] & PAGE_PRESENT)) {
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1;
        pml4_table[pml4_i] = make_entry(phys_of(tbl), PAGE_WRITE);
    }
    uint64_t *pdpt = get_table(pml4_table[pml4_i]);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1;
        pdpt[pdpt_i] = make_entry(phys_of(tbl), PAGE_WRITE);
    }
    uint64_t *pd = get_table(pdpt[pdpt_i]);

    if (flags & PAGE_HUGE) {
        pd[pd_i] = make_entry(phys, flags | PAGE_HUGE | PAGE_WRITE);
        return 0;
    }

    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1;
        pd[pd_i] = make_entry(phys_of(tbl), PAGE_WRITE);
    }
    uint64_t *pt = get_table(pd[pd_i]);

    pt[pt_i] = make_entry(phys, flags | PAGE_WRITE);
    invlpg((void *)virt);
    return 0;
}

void unmap_page(virt_addr_t virt) {
    uint16_t pml4_i = PML4_INDEX(virt);
    uint16_t pdpt_i = PDPT_INDEX(virt);
    uint16_t pd_i   = PD_INDEX(virt);
    uint16_t pt_i   = PT_INDEX(virt);

    if (!(pml4_table[pml4_i] & PAGE_PRESENT)) return;
    uint64_t *pdpt = get_table(pml4_table[pml4_i]);
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return;
    uint64_t *pd = get_table(pdpt[pdpt_i]);
    if (!(pd[pd_i] & PAGE_PRESENT)) return;

    if (pd[pd_i] & PAGE_HUGE) {
        pd[pd_i] = 0;
        invlpg((void *)virt);
        return;
    }

    uint64_t *pt = get_table(pd[pd_i]);
    pt[pt_i] = 0;
    invlpg((void *)virt);
}

static inline void load_cr3(phys_addr_t phys) { __asm__ volatile("mov %0, %%cr3" :: "r"(phys) : "memory"); }

void paging_init(phys_addr_t kernel_start, phys_addr_t kernel_end) {
    memset_local(pml4_table, 0, sizeof(pml4_table));
    memset_local(pdpt_low, 0, sizeof(pdpt_low));
    memset_local(pd_low, 0, sizeof(pd_low));
    memset_local(pdpt_high, 0, sizeof(pdpt_high));
    memset_local(pd_high, 0, sizeof(pd_high));

    /* Low identity map using 2MiB pages */
    pml4_table[0] = make_entry(phys_of(pdpt_low), PAGE_WRITE);
    pdpt_low[0] = make_entry(phys_of(pd_low), PAGE_WRITE);
    for (int i = 0; i < 512; ++i) {
        uint64_t base = (uint64_t)i * 0x200000ULL;
        pd_low[i] = make_entry(base, PAGE_WRITE | PAGE_HUGE);
    }

    /* High-half mapping for kernel */
    size_t pml4_hi = PML4_INDEX(KERNEL_BASE);
    size_t pdpt_hi = PDPT_INDEX(KERNEL_BASE);
    pml4_table[pml4_hi] = make_entry(phys_of(pdpt_high), PAGE_WRITE);
    pdpt_high[pdpt_hi] = make_entry(phys_of(pd_high), PAGE_WRITE);

    phys_addr_t k_phys = (kernel_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    phys_addr_t k_end  = (kernel_end   + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (phys_addr_t p = k_phys; p < k_end; p += 0x200000ULL) {
        virt_addr_t v = KERNEL_BASE + (p - k_phys);
        size_t pd_index = PD_INDEX(v);
        pd_high[pd_index] = make_entry(p, PAGE_WRITE | PAGE_HUGE);
    }

    /* pml4_table lives at higher-half; convert to physical for CR3 */
    load_cr3(phys_of(pml4_table));
}
