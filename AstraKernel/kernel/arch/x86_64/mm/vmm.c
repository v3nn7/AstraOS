#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "string.h"
#include "kernel.h"
#include "stddef.h"

extern volatile struct limine_hhdm_request limine_hhdm_request;
extern char _kernel_start_physical[];
extern char _kernel_end_physical[];

#define PML4_INDEX(x)   (((x) >> 39) & 0x1FF)
#define PDPT_INDEX(x)   (((x) >> 30) & 0x1FF)
#define PD_INDEX(x)     (((x) >> 21) & 0x1FF)
#define PT_INDEX(x)     (((x) >> 12) & 0x1FF)

static uint64_t pml4_phys = 0;
static uint64_t *pml4 = NULL;

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
static inline uint64_t align_down_u64(uint64_t v, uint64_t a) { return v & ~(a - 1); }
static inline uint64_t phys_of(void *virt) { return (uint64_t)virt - pmm_hhdm_offset; }
static inline void *virt_of(uint64_t phys) { return (void *)(phys + pmm_hhdm_offset); }

static inline uint64_t make_entry(uint64_t phys, uint64_t flags) {
    return (phys & ~0xFFFULL) | (flags | PAGE_PRESENT);
}

static uint64_t alloc_table(void) {
    uint64_t phys = (uint64_t)pmm_alloc_page();
    if (!phys) {
        printf("VMM: out of memory for page tables\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    k_memset(virt_of(phys), 0, PAGE_SIZE);
    return phys;
}

static inline uint64_t *get_table(uint64_t entry) {
    return (uint64_t *)virt_of(entry & ~0xFFFULL);
}

static uint64_t *ensure_table(uint64_t *parent, size_t idx, uint64_t flags) {
    if (!(parent[idx] & PAGE_PRESENT)) {
        uint64_t phys = alloc_table();
        parent[idx] = make_entry(phys, PAGE_WRITE | flags);
    }
    return get_table(parent[idx]);
}

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void write_cr3(uint64_t v) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(v) : "memory");
}

static inline void enable_pse(void) {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 4);
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
}

static void map_huge(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pdpt = ensure_table(pml4, PML4_INDEX(virt), PAGE_WRITE);
    uint64_t *pd   = ensure_table(pdpt, PDPT_INDEX(virt), PAGE_WRITE);
    pd[PD_INDEX(virt)] = make_entry(phys, flags | PAGE_HUGE | PAGE_WRITE);
    invlpg((void *)virt);
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (flags & PAGE_HUGE) {
        map_huge(virt, align_down_u64(phys, 0x200000ULL), flags);
        return;
    }

    uint64_t *pdpt = ensure_table(pml4, PML4_INDEX(virt), PAGE_WRITE);
    uint64_t *pd   = ensure_table(pdpt, PDPT_INDEX(virt), PAGE_WRITE);

    if ((pd[PD_INDEX(virt)] & PAGE_HUGE) && !(flags & PAGE_HUGE)) {
        /* Split existing huge page into a new PT */
        uint64_t huge_phys = pd[PD_INDEX(virt)] & ~0x1FFFFFULL;
        uint64_t pt_phys = alloc_table();
        uint64_t *pt = (uint64_t *)virt_of(pt_phys);
        for (size_t i = 0; i < 512; ++i) {
            pt[i] = make_entry(huge_phys + (i * PAGE_SIZE), PAGE_WRITE);
        }
        pd[PD_INDEX(virt)] = make_entry(pt_phys, PAGE_WRITE);
    }

    uint64_t *pt = ensure_table(pd, PD_INDEX(virt), PAGE_WRITE);
    pt[PT_INDEX(virt)] = make_entry(phys, flags | PAGE_WRITE);
    invlpg((void *)virt);
}

void vmm_unmap(uint64_t virt) {
    uint64_t *pdpt = pml4 && (pml4[PML4_INDEX(virt)] & PAGE_PRESENT)
                   ? get_table(pml4[PML4_INDEX(virt)]) : NULL;
    if (!pdpt) return;
    uint64_t *pd = (pdpt[PDPT_INDEX(virt)] & PAGE_PRESENT)
                 ? get_table(pdpt[PDPT_INDEX(virt)]) : NULL;
    if (!pd) return;
    if (pd[PD_INDEX(virt)] & PAGE_HUGE) {
        pd[PD_INDEX(virt)] = 0;
        invlpg((void *)virt);
        return;
    }
    uint64_t *pt = (pd[PD_INDEX(virt)] & PAGE_PRESENT)
                 ? get_table(pd[PD_INDEX(virt)]) : NULL;
    if (!pt) return;
    pt[PT_INDEX(virt)] = 0;
    invlpg((void *)virt);
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    if (!pml4) return 0;
    uint64_t pml4e = pml4[PML4_INDEX(virt)];
    if (!(pml4e & PAGE_PRESENT)) return 0;
    uint64_t *pdpt = get_table(pml4e);

    uint64_t pdpte = pdpt[PDPT_INDEX(virt)];
    if (!(pdpte & PAGE_PRESENT)) return 0;
    uint64_t *pd = get_table(pdpte);

    uint64_t pde = pd[PD_INDEX(virt)];
    if (!(pde & PAGE_PRESENT)) return 0;
    if (pde & PAGE_HUGE) {
        uint64_t phys_base = pde & ~0x1FFFFFULL;
        return phys_base + (virt & 0x1FFFFFULL);
    }

    uint64_t *pt = get_table(pde);
    uint64_t pte = pt[PT_INDEX(virt)];
    if (!(pte & PAGE_PRESENT)) return 0;
    return (pte & ~0xFFFULL) + (virt & 0xFFFULL);
}

void *vmm_map_dma(void *phys_ptr, size_t size) {
    uint64_t phys = (uint64_t)phys_ptr;
    uint64_t offset = phys & (PAGE_SIZE - 1);
    uint64_t aligned_phys = align_down_u64(phys, PAGE_SIZE);
    size_t length = align_up_u64(size + offset, PAGE_SIZE);
    uint64_t virt_base = pmm_hhdm_offset + aligned_phys;

    for (size_t i = 0; i < length; i += PAGE_SIZE) {
        vmm_map(virt_base + i, aligned_phys + i, VMM_FLAGS_DEVICE);
    }
    return (void *)(virt_base + offset);
}

static void map_identity_low_4g(void) {
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += 0x200000ULL) {
        vmm_map(addr, addr, PAGE_WRITE | PAGE_HUGE | PAGE_GLOBAL);
    }
}

static void map_kernel_image(void) {
    uint64_t k_start = align_down_u64((uint64_t)_kernel_start_physical, 0x200000ULL);
    uint64_t k_end   = align_up_u64((uint64_t)_kernel_end_physical,   0x200000ULL);
    for (uint64_t phys = k_start; phys < k_end; phys += 0x200000ULL) {
        uint64_t virt = KERNEL_BASE + phys;
        vmm_map(virt, phys, PAGE_WRITE | PAGE_HUGE | PAGE_GLOBAL);
    }
}

static void map_hhdm_space(void) {
    uint64_t max_phys = align_up_u64(pmm_max_physical, 0x200000ULL);
    for (uint64_t phys = 0; phys < max_phys; phys += 0x200000ULL) {
        uint64_t virt = pmm_hhdm_offset + phys;
        vmm_map(virt, phys, PAGE_WRITE | PAGE_HUGE);
    }
}

void vmm_init(void) {
    if (!pmm_hhdm_offset) {
        printf("VMM: HHDM offset not set\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    enable_pse();

    pml4_phys = (uint64_t)pmm_alloc_page();
    if (!pml4_phys) {
        printf("VMM: cannot allocate PML4\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    pml4 = (uint64_t *)virt_of(pml4_phys);
    k_memset(pml4, 0, PAGE_SIZE);

    map_identity_low_4g();
    map_kernel_image();
    map_hhdm_space();

    write_cr3(pml4_phys);
}

void vmm_page_fault_handler(interrupt_frame_t *frame, uint64_t code) {
    uint64_t fault_addr = read_cr2();
    printf("Page fault: addr=%p code=%llx rip=%p rsp=%p\n",
           (void *)fault_addr,
           (unsigned long long)code,
           (void *)frame->rip,
           (void *)frame->rsp);
    while (1) { __asm__ volatile("cli; hlt"); }
}
