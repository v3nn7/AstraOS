/**
 * Simple DMA allocator: contiguous, page-aligned, below 4GB.
 */

#include "dma.h"
#include "pmm.h"
#include "klog.h"

#define PAGE_SIZE 4096ULL

static inline uintptr_t phys_to_virt(uintptr_t phys) {
    if (pmm_hhdm_offset) return phys + pmm_hhdm_offset;
    return phys;
}

void *dma_alloc(size_t size, size_t align, phys_addr_t *phys_out) {
    if (size == 0) return NULL;
    if (align < PAGE_SIZE) align = PAGE_SIZE;
    /* round up size to pages */
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    while (1) {
        /* find aligned first page */
        phys_addr_t first = 0;
        do {
            void *p = pmm_alloc_page();
            if (!p) return NULL;
            first = (phys_addr_t)p;
            if ((first % align) != 0 || first >= 0x100000000ULL) {
                pmm_free_page((void*)first);
                first = 0;
            }
        } while (first == 0);

        /* try to grab contiguous pages */
        bool ok = true;
        for (size_t i = 1; i < pages; ++i) {
            phys_addr_t want = first + i * PAGE_SIZE;
            void *p = pmm_alloc_page();
            if (!p) { ok = false; break; }
            phys_addr_t got = (phys_addr_t)p;
            if (got != want || got >= 0x100000000ULL) {
                /* fail: free everything and restart */
                pmm_free_page((void*)got);
                for (size_t j = 0; j < i; ++j) {
                    pmm_free_page((void*)(first + j * PAGE_SIZE));
                }
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        if (phys_out) *phys_out = first;
        return (void*)phys_to_virt(first);
    }
}

void dma_free(void *virt, size_t size) {
    if (!virt || size == 0) return;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t phys = (uintptr_t)virt;
    if (pmm_hhdm_offset && phys >= pmm_hhdm_offset) {
        phys -= pmm_hhdm_offset;
    }
    for (size_t i = 0; i < pages; ++i) {
        pmm_free_page((void*)(phys + i * PAGE_SIZE));
    }
}

