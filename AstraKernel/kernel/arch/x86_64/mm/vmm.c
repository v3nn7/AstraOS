#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "string.h"
#include "kernel.h"
#include "stddef.h"

extern volatile struct limine_hhdm_request limine_hhdm_request;
extern volatile struct limine_executable_address_request limine_exec_addr_request;
extern volatile struct limine_framebuffer_request limine_fb_request;
extern volatile struct limine_memmap_request limine_memmap_request;

extern char _kernel_start[];
extern char _kernel_end[];

#define PML4_IDX(x)   (((x) >> 39) & 0x1FF)
#define PDPT_IDX(x)   (((x) >> 30) & 0x1FF)
#define PD_IDX(x)     (((x) >> 21) & 0x1FF)
#define PT_IDX(x)     (((x) >> 12) & 0x1FF)

static uint64_t pml4_phys = 0;
static uint64_t *pml4 = NULL;

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
static inline uint64_t align_down_u64(uint64_t v, uint64_t a) { return v & ~(a - 1); }

static inline void *virt_of(uint64_t phys) { return (void *)(phys + pmm_hhdm_offset); }
static inline uint64_t phys_of(void *virt) { return (uint64_t)virt - pmm_hhdm_offset; }

static inline uint64_t make_entry(uint64_t phys, uint64_t flags) {
    return (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void write_cr3(uint64_t v) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(v) : "memory");
}

static uint64_t alloc_table(void) {
    uint64_t phys = (uint64_t)pmm_alloc_page();
    if (!phys) {
        printf("VMM: out of memory for PT\n");
        while (1) __asm__("cli; hlt");
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
        parent[idx] = make_entry(phys, flags | PAGE_WRITE);
    }
    return get_table(parent[idx]);
}

static inline void invlpg_wrap(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

/* -------------------------------------------------------
   PRECISE FRAMEBUFFER EXCLUSION
------------------------------------------------------- */
static bool is_addr_in_fb(uint64_t addr, uint64_t fb_phys, uint64_t fb_size) {
    if (!fb_phys || !fb_size) return false;
    /* Align to 2MiB so we never split the FB region with huge pages */
    uint64_t fb_start = align_down_u64(fb_phys, 0x200000);
    uint64_t fb_end   = align_up_u64(fb_phys + fb_size, 0x200000);
    return addr >= fb_start && addr < fb_end;
}

/* -------------------------------------------------------
   MAP HUGE (2MB)
------------------------------------------------------- */
static void map_huge(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pdpt = ensure_table(pml4, PML4_IDX(virt), PAGE_WRITE);
    uint64_t *pd   = ensure_table(pdpt, PDPT_IDX(virt), PAGE_WRITE);

    pd[PD_IDX(virt)] = make_entry(phys, flags | PAGE_HUGE | PAGE_WRITE);
    invlpg_wrap(virt);
}

/* -------------------------------------------------------
   MAP 4KB PAGE
------------------------------------------------------- */
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (flags & PAGE_HUGE) {
        map_huge(virt, align_down_u64(phys, 0x200000), flags);
        return;
    }

    uint64_t *pdpt = ensure_table(pml4, PML4_IDX(virt), PAGE_WRITE);
    uint64_t *pd   = ensure_table(pdpt, PDPT_IDX(virt), PAGE_WRITE);

    if (pd[PD_IDX(virt)] & PAGE_HUGE) {
        uint64_t huge_phys = pd[PD_IDX(virt)] & ~0x1FFFFFULL;
        uint64_t pt_phys = alloc_table();
        uint64_t *pt = virt_of(pt_phys);

        for (size_t i = 0; i < 512; i++)
            pt[i] = make_entry(huge_phys + i * PAGE_SIZE, PAGE_WRITE);

        pd[PD_IDX(virt)] = make_entry(pt_phys, PAGE_WRITE);
    }

    uint64_t *pt = ensure_table(pd, PD_IDX(virt), PAGE_WRITE);
    pt[PT_IDX(virt)] = make_entry(phys, flags | PAGE_WRITE);
    invlpg_wrap(virt);
}

/* -------------------------------------------------------
   UNMAP 4KB OR HUGE PAGE
------------------------------------------------------- */
void vmm_unmap(uint64_t virt) {
    uint64_t *pdpt = (pml4 && (pml4[PML4_IDX(virt)] & PAGE_PRESENT))
                   ? get_table(pml4[PML4_IDX(virt)]) : NULL;
    if (!pdpt) return;

    uint64_t *pd = (pdpt[PDPT_IDX(virt)] & PAGE_PRESENT)
                 ? get_table(pdpt[PDPT_IDX(virt)]) : NULL;
    if (!pd) return;

    if (pd[PD_IDX(virt)] & PAGE_HUGE) {
        pd[PD_IDX(virt)] = 0;
        invlpg_wrap(virt);
        return;
    }

    uint64_t *pt = (pd[PD_IDX(virt)] & PAGE_PRESENT)
                 ? get_table(pd[PD_IDX(virt)]) : NULL;
    if (!pt) return;

    pt[PT_IDX(virt)] = 0;
    invlpg_wrap(virt);
}

/* -------------------------------------------------------
   VIRT -> PHYS QUERY
------------------------------------------------------- */
uint64_t vmm_virt_to_phys(uint64_t virt) {
    if (!pml4) return 0;

    uint64_t pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & PAGE_PRESENT)) return 0;
    uint64_t *pdpt = get_table(pml4e);

    uint64_t pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & PAGE_PRESENT)) return 0;
    uint64_t *pd = get_table(pdpte);

    uint64_t pde = pd[PD_IDX(virt)];
    if (!(pde & PAGE_PRESENT)) return 0;
    if (pde & PAGE_HUGE) {
        uint64_t phys_base = pde & ~0x1FFFFFULL;
        return phys_base + (virt & 0x1FFFFFULL);
    }

    uint64_t *pt = get_table(pde);
    uint64_t pte = pt[PT_IDX(virt)];
    if (!(pte & PAGE_PRESENT)) return 0;
    return (pte & ~0xFFFULL) + (virt & 0xFFFULL);
}

/* -------------------------------------------------------
   DMA MAP VIA HHDM
------------------------------------------------------- */
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

/* -------------------------------------------------------
   IDENTITY MAP 0–4GB (HUGE PAGES, FB-SAFE SPLIT)
------------------------------------------------------- */
static void identity_map_low_4g_huge_safe(void) {
    struct limine_framebuffer *fb = NULL;
    uint64_t fb_phys = 0, fb_size = 0, fb_start = 0, fb_end = 0;

    /* Pobierz framebuffer i wyznacz jego fizyczny zakres */
    if (limine_fb_request.response) {
        fb = limine_fb_request.response->framebuffers[0];
        if (fb) {
            uint64_t fb_virt = (uint64_t)fb->address; /* Limine podaje VA (zwykle HHDM) */
            fb_phys = (fb_virt >= pmm_hhdm_offset) ? (fb_virt - pmm_hhdm_offset) : fb_virt;

            /* Użyj dokładnej długości z Limine memmap jeśli dostępna */
            uint64_t fb_len = 0;
            if (limine_memmap_request.response) {
                struct limine_memmap_response *mm = limine_memmap_request.response;
                for (uint64_t i = 0; i < mm->entry_count; i++) {
                    struct limine_memmap_entry *e = mm->entries[i];
                    if (e->type == LIMINE_MEMMAP_FRAMEBUFFER && e->base == fb_phys) {
                        fb_len = e->length;
                        break;
                    }
                }
            }
            if (fb_len == 0) {
                fb_len = align_up_u64((uint64_t)fb->pitch * fb->height, PAGE_SIZE);
            }
            fb_size = fb_len;
            fb_start = fb_phys;
            fb_end   = fb_phys + fb_size;
        }
    }

    /* Idź co 2MiB – huge pages tam, gdzie nie nachodzą na FB; w kolizji 4KiB */
    for (uint64_t block = 0; block < 0x100000000ULL; block += 0x200000ULL) {
        uint64_t block_end = block + 0x200000ULL;

        bool overlap = fb_size && !(block_end <= fb_start || block >= fb_end);
        if (!overlap) {
            vmm_map(block, block, PAGE_WRITE | PAGE_GLOBAL | PAGE_HUGE);
            continue;
        }

        /* Kolizja z FB → rozbij na 4KiB, pomijając sam framebuffer */
        for (uint64_t p = block; p < block_end; p += PAGE_SIZE) {
            if (p >= fb_start && p < fb_end)
                continue;
            vmm_map(p, p, PAGE_WRITE | PAGE_GLOBAL);
        }
    }
}

/* -------------------------------------------------------
   MAP KERNEL IMAGE EXACTLY AS LOADED BY LIMINE
------------------------------------------------------- */
static void map_kernel_image(void) {
    struct limine_executable_address_response *ex = limine_exec_addr_request.response;

    uint64_t phys = ex->physical_base;
    uint64_t virt = ex->virtual_base;

    uint64_t size = (uint64_t)_kernel_end - (uint64_t)_kernel_start;
    /* Mapuj faktyczny rozmiar kernela + guard dla .bss i przyszłych sekcji */
    /* Guard 64MB powinien wystarczyć dla .bss, initcalls i małych rozszerzeń */
    uint64_t size_safe = align_up_u64(size, PAGE_SIZE) + (64ULL * 1024 * 1024); /* +64 MiB guard */

    uint64_t phys_start = align_down_u64(phys, PAGE_SIZE);
    uint64_t virt_start = align_down_u64(virt, PAGE_SIZE);
    uint64_t virt_end   = align_up_u64(virt + size_safe, PAGE_SIZE);
    
    /* Wyznacz zakres framebuffer'a (jeśli dostępny), aby go pominąć */
    uint64_t fb_start = 0, fb_end = 0;
    if (limine_fb_request.response) {
        struct limine_framebuffer *fb = limine_fb_request.response->framebuffers[0];
        if (fb) {
            uint64_t fb_virt = (uint64_t)fb->address;
            uint64_t fb_phys = (fb_virt >= pmm_hhdm_offset) ? (fb_virt - pmm_hhdm_offset) : fb_virt;
            uint64_t fb_len = 0;
            if (limine_memmap_request.response) {
                struct limine_memmap_response *mm = limine_memmap_request.response;
                for (uint64_t i = 0; i < mm->entry_count; i++) {
                    struct limine_memmap_entry *e = mm->entries[i];
                    if (e->type == LIMINE_MEMMAP_FRAMEBUFFER && e->base == fb_phys) {
                        fb_len = e->length;
                        break;
                    }
                }
            }
            if (fb_len == 0) {
                fb_len = align_up_u64((uint64_t)fb->pitch * fb->height, PAGE_SIZE);
            }
            fb_start = fb_phys;
            fb_end   = fb_phys + fb_len;
        }
    }

    /* CRITICAL: Allocate physical pages for .bss and unmapped kernel sections */
    /* Bootloader only loads sections with AT() load address, .bss is zero-initialized but not loaded */
    uint64_t virt_cur = virt_start;
    uint64_t phys_cur = phys_start;
    
    /* Map loaded sections first (up to _kernel_end) */
    uint64_t loaded_size = (uint64_t)_kernel_end - (uint64_t)_kernel_start;
    uint64_t loaded_end_virt = virt_start + loaded_size;
    
    printf("VMM: mapping loaded sections: virt_start=%llx loaded_end=%llx\n",
           (unsigned long long)virt_start, (unsigned long long)loaded_end_virt);
    
    size_t mapped_loaded = 0;
    for (; virt_cur < loaded_end_virt; virt_cur += PAGE_SIZE, phys_cur += PAGE_SIZE) {
        /* Skip framebuffer if it overlaps */
        uint64_t cur_phys = phys_cur;
        if (fb_end && cur_phys >= fb_start && cur_phys < fb_end) {
            continue;
        }
        vmm_map(virt_cur, cur_phys, PAGE_WRITE | PAGE_GLOBAL);
        mapped_loaded++;
        if (mapped_loaded % 100 == 0) {
            printf("VMM: mapped %zu loaded pages...\n", mapped_loaded);
        }
    }
    printf("VMM: mapped %zu loaded pages total\n", mapped_loaded);
    
    /* Allocate and map physical pages for .bss and guard region */
    printf("VMM: allocating pages for .bss/guard: virt_cur=%llx virt_end=%llx\n",
           (unsigned long long)virt_cur, (unsigned long long)virt_end);
    size_t allocated_bss = 0;
    for (; virt_cur < virt_end; virt_cur += PAGE_SIZE) {
        void *page_phys = pmm_alloc_page();
        if (!page_phys) {
            printf("VMM: WARNING: failed to allocate page for kernel VA=%llx (allocated %zu so far)\n",
                   (unsigned long long)virt_cur, allocated_bss);
            break;
        }
        uint64_t page_phys_addr = (uint64_t)page_phys;
        k_memset((void *)(page_phys_addr + pmm_hhdm_offset), 0, PAGE_SIZE); /* Zero-initialize .bss pages */
        vmm_map(virt_cur, page_phys_addr, PAGE_WRITE | PAGE_GLOBAL);
        allocated_bss++;
        if (allocated_bss % 100 == 0 && allocated_bss <= 1000) {
            printf("VMM: allocated %zu .bss pages...\n", allocated_bss);
        }
    }
    printf("VMM: allocated %zu .bss/guard pages total\n", allocated_bss);

    printf("VMM: kernel mapped VA=%llx PHYS=%llx size=%llx (fb excluded if overlapped)\n",
           (unsigned long long)virt_start,
           (unsigned long long)phys_start,
           (unsigned long long)(virt_end - virt_start));
}

/* Helper: map a physical region into HHDM with mixed 4KiB/2MiB pages.
   Always align down/up so the full region is covered. */
static void map_hhdm_region(uint64_t base, uint64_t end, uint64_t flags) {
    uint64_t phys = align_down_u64(base, PAGE_SIZE);
    uint64_t pend = align_up_u64(end, PAGE_SIZE);

    /* Lead-in to next 2MiB boundary */
    uint64_t aligned2m = align_up_u64(phys, 0x200000);
    uint64_t lead_end = (aligned2m < pend) ? aligned2m : pend;
    for (; phys < lead_end; phys += PAGE_SIZE) {
        vmm_map(pmm_hhdm_offset + phys, phys, flags);
    }
    /* 2MiB chunks */
    for (; phys + 0x200000 <= pend; phys += 0x200000) {
        vmm_map(pmm_hhdm_offset + phys, phys, flags | PAGE_HUGE);
    }
    /* Tail */
    for (; phys < pend; phys += PAGE_SIZE) {
        vmm_map(pmm_hhdm_offset + phys, phys, flags);
    }
}

/* Helper: strict 4KiB mapping (no huge pages), to guarantee full coverage. */
static void map_hhdm_region_4k(uint64_t base, uint64_t end, uint64_t flags) {
    uint64_t phys = align_down_u64(base, PAGE_SIZE);
    uint64_t pend = align_up_u64(end, PAGE_SIZE);
    for (; phys < pend; phys += PAGE_SIZE) {
        vmm_map(pmm_hhdm_offset + phys, phys, flags);
    }
}

/* -------------------------------------------------------
   MAP HHDM = direct map of RAM
------------------------------------------------------- */
static void map_hhdm(void) {
    /* Prefer precise HHDM mapping based on Limine memmap */
    if (limine_memmap_request.response) {
        struct limine_memmap_response *mm = limine_memmap_request.response;
        for (uint64_t i = 0; i < mm->entry_count; i++) {
            struct limine_memmap_entry *e = mm->entries[i];
            uint64_t base = e->base;
            uint64_t end  = e->base + e->length;

            switch (e->type) {
                case LIMINE_MEMMAP_USABLE:
                    /* USABLE: map w 4KiB granularity to pokryć cały region */
                    map_hhdm_region_4k(base, end, PAGE_WRITE);
                    break;
                case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
                case LIMINE_MEMMAP_ACPI_NVS:
                case LIMINE_MEMMAP_ACPI_TABLES:
                case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
                    /* Map regions that early code may access via HHDM */
                    map_hhdm_region(base, end, PAGE_WRITE);
                    break;
                case LIMINE_MEMMAP_FRAMEBUFFER:
                    /* Framebuffer: tylko 4KiB, uncached */
                    map_hhdm_region_4k(base, end, PAGE_WRITE | PAGE_CACHE_DISABLE);
                    break;
                default:
                    /* Do not map RESERVED/BAD/etc into HHDM by default */
                    break;
            }
        }
        return;
    }

    /* Fallback: map up to max physical using 2MiB pages */
    uint64_t top = align_up_u64(pmm_max_physical, 0x200000);
    for (uint64_t phys = 0; phys < top; phys += 0x200000) {
        uint64_t virt = pmm_hhdm_offset + phys;
        vmm_map(virt, phys, PAGE_WRITE | PAGE_HUGE);
    }
}

/* -------------------------------------------------------
   Explicitly map framebuffer (4KiB, uncached)
------------------------------------------------------- */
static void map_framebuffer_explicit(void) {
    if (!limine_fb_request.response) return;
    struct limine_framebuffer *fb = limine_fb_request.response->framebuffers[0];
    if (!fb) return;

    uint64_t fb_virt = (uint64_t)fb->address;
    uint64_t fb_phys = (fb_virt >= pmm_hhdm_offset) ? (fb_virt - pmm_hhdm_offset) : fb_virt;

    uint64_t fb_len = 0;
    /* Prefer memmap entry length if available */
    if (limine_memmap_request.response) {
        struct limine_memmap_response *mm = limine_memmap_request.response;
        for (uint64_t i = 0; i < mm->entry_count; i++) {
            struct limine_memmap_entry *e = mm->entries[i];
            if (e->type == LIMINE_MEMMAP_FRAMEBUFFER && e->base == fb_phys) {
                fb_len = e->length;
                break;
            }
        }
    }
    if (fb_len == 0) {
        fb_len = align_up_u64((uint64_t)fb->pitch * fb->height, PAGE_SIZE);
    }

    uint64_t start = align_down_u64(fb_phys, PAGE_SIZE);
    uint64_t end   = align_up_u64(fb_phys + fb_len, PAGE_SIZE);
    for (uint64_t p = start; p < end; p += PAGE_SIZE) {
        vmm_map(pmm_hhdm_offset + p, p, PAGE_WRITE | PAGE_CACHE_DISABLE);
    }
}

/* -------------------------------------------------------
   INIT VMM
------------------------------------------------------- */
void vmm_init(void) {
    if (!limine_exec_addr_request.response) {
        printf("VMM: missing executable address response\n");
        while (1) __asm__("cli; hlt");
    }
    if (!pmm_hhdm_offset) {
        printf("VMM: HHDM offset not set\n");
        while (1) __asm__("cli; hlt");
    }

    uint64_t cr3_before = read_cr3();
    printf("VMM: init start cr3_before=%llx\n", (unsigned long long)cr3_before);

    pml4_phys = (uint64_t)pmm_alloc_page();
    pml4 = virt_of(pml4_phys);
    k_memset(pml4, 0, PAGE_SIZE);
    printf("VMM: pml4_phys=%llx\n", (unsigned long long)pml4_phys);

    identity_map_low_4g_huge_safe();
    printf("VMM: identity mapped (huge, FB-safe)\n");
    map_kernel_image();
    printf("VMM: kernel mapped\n");
    map_hhdm();
    printf("VMM: hhdm mapped\n");
    map_framebuffer_explicit();
    printf("VMM: framebuffer mapped (explicit)\n");

    /* Mapuj LAPIC/IOAPIC w HHDM, bo nie leżą w RAM */
    vmm_map(pmm_hhdm_offset + 0xFEE00000, 0xFEE00000,
            PAGE_WRITE | PAGE_CACHE_DISABLE);
    vmm_map(pmm_hhdm_offset + 0xFEC00000, 0xFEC00000,
            PAGE_WRITE | PAGE_CACHE_DISABLE);
    printf("VMM: apic mapped\n");

    write_cr3(pml4_phys);
    uint64_t cr3_after = read_cr3();
    printf("VMM: cr3 switched -> %llx\n", (unsigned long long)cr3_after);

    printf("VMM: ready\n");
}

/* -------------------------------------------------------
   PAGE FAULT
------------------------------------------------------- */
void vmm_page_fault_handler(interrupt_frame_t *f, uint64_t code) {
    uint64_t cr2;
    __asm__("mov %%cr2, %0" : "=r"(cr2));

    uint64_t phys = vmm_virt_to_phys(cr2);
    
    /* Check if fault is in kernel space and might be lazy-allocatable */
    if (cr2 >= KERNEL_BASE && phys == 0) {
        /* Try to allocate and map this page */
        void *page_phys = pmm_alloc_page();
        if (page_phys) {
            uint64_t page_phys_addr = (uint64_t)page_phys;
            k_memset((void *)(page_phys_addr + pmm_hhdm_offset), 0, PAGE_SIZE);
            vmm_map(cr2, page_phys_addr, PAGE_WRITE | PAGE_GLOBAL);
            printf("VMM: auto-mapped kernel page VA=%p PHYS=%p\n", (void *)cr2, (void *)page_phys_addr);
            return; /* Continue execution */
        }
    }
    
    printf("PAGE FAULT at %p (code=%x) rip=%p, phys=%p\n",
           (void *)cr2, (uint32_t)code, (void *)f->rip, (void *)phys);

    while (1) __asm__("cli; hlt");
}
