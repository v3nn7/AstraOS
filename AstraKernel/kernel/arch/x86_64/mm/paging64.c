#include "types.h"
#include "memory.h"
#include "interrupts.h"

/* Upewnij się, że KERNEL_BASE jest zdefiniowane w types.h lub memory.h, np.:
 * #define KERNEL_BASE 0xFFFFFFFF80000000ULL
 */

#define PML4_INDEX(x)   (((x) >> 39) & 0x1FF)
#define PDPT_INDEX(x)   (((x) >> 30) & 0x1FF)
#define PD_INDEX(x)     (((x) >> 21) & 0x1FF)
#define PT_INDEX(x)     (((x) >> 12) & 0x1FF)

/* Flagi stron - upewnij się, że są zdefiniowane */
#ifndef PAGE_PRESENT
#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_HUGE    0x80
#endif

/* Statyczne tablice stronicowania dla bootstrapu */
static uint64_t pml4_table[512] __attribute__((aligned(PAGE_SIZE)));
static uint64_t pdpt_low[512]   __attribute__((aligned(PAGE_SIZE)));
static uint64_t pd_low[512]     __attribute__((aligned(PAGE_SIZE)));
static uint64_t pdpt_high[512]  __attribute__((aligned(PAGE_SIZE)));
static uint64_t pd_high[512]    __attribute__((aligned(PAGE_SIZE)));

/* Prosta pula dla dynamicznie alokowanych tablic (PT) */
#define TABLE_POOL_SIZE 64
static uint64_t table_pool[TABLE_POOL_SIZE][512] __attribute__((aligned(PAGE_SIZE)));
static size_t table_pool_used = 0;

static void *memset_local(void *dst, int c, size_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

/* Konwersja Wirtualny -> Fizyczny (dla pamięci kernela zmapowanej liniowo) */
static inline phys_addr_t phys_of(void *ptr) {
    return (phys_addr_t)((uint64_t)ptr - KERNEL_BASE);
}

/* Konwersja Fizyczny -> Wirtualny (dla dostępu do tablic) */
static inline void* virt_of(phys_addr_t phys) {
    return (void*)((uint64_t)phys + KERNEL_BASE);
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

/* Pobranie wskaźnika do następnej tablicy z wpisu */
static uint64_t *get_table_ptr(uint64_t entry) {
    phys_addr_t phys = (phys_addr_t)(entry & ~0xFFFULL);
    return (uint64_t *)virt_of(phys);
}

static inline void load_cr3(phys_addr_t phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}

int map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    uint16_t pml4_i = PML4_INDEX(virt);
    uint16_t pdpt_i = PDPT_INDEX(virt);
    uint16_t pd_i   = PD_INDEX(virt);
    uint16_t pt_i   = PT_INDEX(virt);

    /* Level 4 (PML4) -> Level 3 (PDPT) */
    if (!(pml4_table[pml4_i] & PAGE_PRESENT)) {
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1; // Brak pamięci w puli
        pml4_table[pml4_i] = make_entry(phys_of(tbl), PAGE_WRITE | PAGE_PRESENT);
    }
    uint64_t *pdpt = get_table_ptr(pml4_table[pml4_i]);

    /* Level 3 (PDPT) -> Level 2 (PD) */
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1;
        pdpt[pdpt_i] = make_entry(phys_of(tbl), PAGE_WRITE | PAGE_PRESENT);
    }
    uint64_t *pd = get_table_ptr(pdpt[pdpt_i]);

    /* If a huge page is present but we need 4K, split the 2MB page into a PT */
    if ((pd[pd_i] & PAGE_HUGE) && !(flags & PAGE_HUGE)) {
        uint64_t huge_phys_base = pd[pd_i] & ~0x1FFFFFULL;
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1;
        for (int j = 0; j < 512; ++j) {
            uint64_t p = huge_phys_base + ((uint64_t)j << 12);
            tbl[j] = make_entry(p, PAGE_WRITE | PAGE_PRESENT);
        }
        pd[pd_i] = make_entry(phys_of(tbl), PAGE_WRITE | PAGE_PRESENT);
    }

    /* Obsługa Huge Pages (2MB) */
    if (flags & PAGE_HUGE) {
        pd[pd_i] = make_entry(phys, flags | PAGE_HUGE | PAGE_PRESENT);
        invlpg((void*)virt);
        return 0;
    }

    /* Level 2 (PD) -> Level 1 (PT) */
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t *tbl = alloc_table();
        if (!tbl) return -1;
        pd[pd_i] = make_entry(phys_of(tbl), PAGE_WRITE | PAGE_PRESENT);
    }
    uint64_t *pt = get_table_ptr(pd[pd_i]);

    /* Level 1 (PT) -> Physical Frame */
    pt[pt_i] = make_entry(phys, flags | PAGE_PRESENT);
    invlpg((void*)virt);
    return 0;
}

void unmap_page(virt_addr_t virt) {
    uint16_t pml4_i = PML4_INDEX(virt);
    uint16_t pdpt_i = PDPT_INDEX(virt);
    uint16_t pd_i   = PD_INDEX(virt);
    uint16_t pt_i   = PT_INDEX(virt);

    if (!(pml4_table[pml4_i] & PAGE_PRESENT)) return;
    uint64_t *pdpt = get_table_ptr(pml4_table[pml4_i]);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return;
    uint64_t *pd = get_table_ptr(pdpt[pdpt_i]);

    if (!(pd[pd_i] & PAGE_PRESENT)) return;

    /* Jeśli to Huge Page, czyścimy wpis w PD */
    if (pd[pd_i] & PAGE_HUGE) {
        pd[pd_i] = 0;
        invlpg((void *)virt);
        return;
    }

    uint64_t *pt = get_table_ptr(pd[pd_i]);
    pt[pt_i] = 0;
    invlpg((void *)virt);
}

void paging_init(phys_addr_t kernel_start, phys_addr_t kernel_end) {
    /* Wyzerowanie wszystkich tablic statycznych */
    memset_local(pml4_table, 0, sizeof(pml4_table));
    memset_local(pdpt_low, 0, sizeof(pdpt_low));
    memset_local(pd_low, 0, sizeof(pd_low));
    memset_local(pdpt_high, 0, sizeof(pdpt_high));
    memset_local(pd_high, 0, sizeof(pd_high));

    /* 1. Identity Map (Low Memory): 0-1GB
     * PML4[0] -> PDPT_Low[0] -> PD_Low (Huge Pages)
     */
    pml4_table[0] = make_entry(phys_of(pdpt_low), PAGE_WRITE);
    pdpt_low[0] = make_entry(phys_of(pd_low), PAGE_WRITE);
    
    for (int i = 0; i < 512; ++i) {
        uint64_t addr = (uint64_t)i * 0x200000ULL; // 2MB
        pd_low[i] = make_entry(addr, PAGE_WRITE | PAGE_HUGE);
    }

    /* 1a. Wymuszone zmapowanie 0xB8000 jako 4K (rozbicie pierwszego PD wpisu) */
    {
        /* przygotuj PT dla 0..2MB */
        uint64_t *pt0 = alloc_table();
        if (pt0) {
            /* zastąp pd_low[0] wpisem do PT */
            pd_low[0] = make_entry(phys_of(pt0), PAGE_WRITE | PAGE_PRESENT);
            /* wyczyść PT */
            memset_local(pt0, 0, PAGE_SIZE);
            /* zmapuj VGA tekst: 0xB8000 -> 0xB8000, RW */
            size_t vga_idx = (0xB8000 >> 12) & 0x1FF;
            pt0[vga_idx] = make_entry(0xB8000, PAGE_WRITE | PAGE_PRESENT);
            /* pozostale wpisy opcjonalnie mogą pokryć resztę 0..2MB jako RW */
            for (size_t j = 0; j < 512; ++j) {
                if (pt0[j] == 0) {
                    uint64_t p = ((uint64_t)j) << 12;
                    pt0[j] = make_entry(p, PAGE_WRITE | PAGE_PRESENT);
                }
            }
        }
    }

    /* 2. Kernel Map (High Memory): KERNEL_BASE + P
     * Mapujemy pamięć fizyczną od 'kernel_start' do 'kernel_end'
     * pod adresem wirtualnym: KERNEL_BASE + addr
     */
    size_t pml4_hi = PML4_INDEX(KERNEL_BASE);
    size_t pdpt_hi = PDPT_INDEX(KERNEL_BASE);
    
    pml4_table[pml4_hi] = make_entry(phys_of(pdpt_high), PAGE_WRITE);
    pdpt_high[pdpt_hi] = make_entry(phys_of(pd_high), PAGE_WRITE);
    /* Niektóre adresy high-half trafiają do sąsiedniego wpisu PDPT (510/511),
       ustawiamy oba, by uniknąć braku wpisu. */
    pdpt_high[511] = make_entry(phys_of(pd_high), PAGE_WRITE);
    pdpt_high[510] = make_entry(phys_of(pd_high), PAGE_WRITE);

    phys_addr_t k_phys_start = (kernel_start) & ~(0x200000ULL - 1);
    phys_addr_t k_phys_end   = (kernel_end + 0x200000ULL - 1) & ~(0x200000ULL - 1);

    for (phys_addr_t p = k_phys_start; p < k_phys_end; p += 0x200000ULL) {
        /* FIX: Utrzymujemy spójność z makrem phys_of.
         * Jeśli phys_of(v) = v - KERNEL_BASE, to:
         * v = KERNEL_BASE + p
         */
        virt_addr_t v = KERNEL_BASE + p;
        
        /* Uwaga: To działa poprawnie tylko jeśli kernel mieści się w 1GB (512 wpisów PD)
         * i mieści się w jednym wpisie PDPT. W przeciwnym razie potrzebna pętla po PDPT.
         */
        size_t pd_idx = PD_INDEX(v);
        if (pd_idx < 512) {
             pd_high[pd_idx] = make_entry(p, PAGE_WRITE | PAGE_HUGE);
        }
    }

    /* Tymczasowo zostawiamy CR3 od Limine (działa) — nasze tabele
       pozostają przygotowane do późniejszego przełączenia. */
    // load_cr3(phys_of(pml4_table));
}