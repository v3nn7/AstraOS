#include "dma.h"
#include "pmm.h"
#include "klog.h"

#define PAGE_SIZE    4096ULL
#define DMA_MAX_PHYS 0x100000000ULL

extern uint64_t pmm_hhdm_offset;

extern phys_addr_t virt_to_phys(void* virt);

void* phys_to_virt(phys_addr_t phys) {
    if (pmm_hhdm_offset)
        return (void*)(phys + pmm_hhdm_offset);
    return (void*)(uintptr_t)phys;
}

void paging_map_dma(uintptr_t virt_addr, phys_addr_t phys_addr) {
    (void)virt_addr;
    (void)phys_addr;
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

    phys_addr_t phys = 0;
    void* virt = pmm_alloc_contiguous_dma(pages, align, DMA_MAX_PHYS, &phys);
    if (!virt) {
        klog_printf(KLOG_ERROR,
            "dma: contiguous alloc failed (need %llu pages under 4GB)",
            (unsigned long long)pages
        );
        return NULL;
    }

    if (phys >= DMA_MAX_PHYS) {
        klog_printf(KLOG_ERROR, "dma: allocated block above 4GB (phys=%p)", (void*)phys);
        pmm_free_contiguous(phys, pages);
        return NULL;
    }

    for (size_t i = 0; i < pages; i++)
        paging_map_dma((uintptr_t)virt + i * PAGE_SIZE, phys + i * PAGE_SIZE);

    if (phys_out)
        *phys_out = phys;

    klog_printf(KLOG_INFO, "dma: allocated size=%zu pages=%zu phys=%p virt=%p",
                size, pages, (void*)phys, virt);

    return virt;
}

void dma_free(void* virt, size_t size) {
    if (!virt || size == 0) return;

    phys_addr_t phys = virt_to_phys(virt);
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    pmm_free_contiguous(phys, pages);
}