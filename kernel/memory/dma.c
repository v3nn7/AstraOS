#include "dma.h"
#include "pmm.h"
#include "klog.h"

#define PAGE_SIZE    4096ULL
#define DMA_MAX_PHYS 0x100000000ULL

/* VMM flags (kept here to avoid C++ headers in C files). */
#define VMM_PRESENT        (1u << 0)
#define VMM_WRITABLE       (1u << 1)
#define VMM_WRITE_THROUGH  (1u << 3)
#define VMM_NO_CACHE       (1u << 4)

extern uint64_t pmm_hhdm_offset;

extern phys_addr_t virt_to_phys(void* virt);

void* phys_to_virt(phys_addr_t phys) {
    if (pmm_hhdm_offset)
        return (void*)(phys + pmm_hhdm_offset);
    return (void*)(uintptr_t)phys;
}

extern void vmm_map(uintptr_t virt, uintptr_t phys, uint32_t flags);

void paging_map_dma(uintptr_t virt_addr, phys_addr_t phys_addr) {
    /* Kernel-only, writable, uncached, write-through mapping for DMA buffers. */
    vmm_map(virt_addr, phys_addr, VMM_WRITABLE | VMM_NO_CACHE | VMM_WRITE_THROUGH | VMM_PRESENT);
    if (virt_addr != phys_addr) {
        vmm_map(phys_addr, phys_addr, VMM_WRITABLE | VMM_NO_CACHE | VMM_WRITE_THROUGH | VMM_PRESENT);
    }
}

static void pmm_free_contiguous(phys_addr_t phys, size_t pages) {
    for (size_t i = 0; i < pages; ++i) {
        void* virt = phys_to_virt(phys + i * PAGE_SIZE);
        pmm_free_page(virt);
    }
}

void* dma_alloc(size_t size, size_t align, phys_addr_t* phys_out) {
    if (size == 0) return NULL;

    if (align < 64) align = 64;

    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    const phys_addr_t DMA_MIN_PHYS = 0x100000ULL;

    phys_addr_t phys = 0;
    void* virt = pmm_alloc_contiguous_dma(pages, align, DMA_MAX_PHYS, &phys);
    if (!virt) {
        klog_printf(KLOG_ERROR,
            "dma: contiguous alloc failed (need %llu pages under 4GB, above 1MB)",
            (unsigned long long)pages
        );
        return NULL;
    }

    if (phys < DMA_MIN_PHYS) {
        klog_printf(KLOG_ERROR, "dma: allocated block below 1MB (phys=0x%llx)", (unsigned long long)phys);
        pmm_free_contiguous(phys, pages);
        return NULL;
    }

    if (phys >= DMA_MAX_PHYS) {
        klog_printf(KLOG_ERROR, "dma: allocated block above 4GB (phys=0x%llx)", (unsigned long long)phys);
        pmm_free_contiguous(phys, pages);
        return NULL;
    }

    for (size_t i = 0; i < pages; i++) {
        uintptr_t page_virt = (uintptr_t)virt + i * PAGE_SIZE;
        phys_addr_t page_phys = phys + i * PAGE_SIZE;
        paging_map_dma(page_virt, page_phys);
    }

    if (phys_out)
        *phys_out = phys;

    klog_printf(KLOG_INFO, "dma: allocated size=%zu pages=%zu phys=0x%llx virt=0x%llx",
                size, pages, (unsigned long long)phys, (unsigned long long)(uintptr_t)virt);

    return virt;
}

void dma_free(void* virt, size_t size) {
    if (!virt || size == 0) return;

    phys_addr_t phys = virt_to_phys(virt);
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    pmm_free_contiguous(phys, pages);
}