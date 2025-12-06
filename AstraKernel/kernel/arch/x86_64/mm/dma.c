#include "dma.h"
#include "pmm.h"
#include "memory.h"
#include "vmm.h"
#include "stddef.h"

void dma_init(void) {
    /* Nothing to do for now */
}

void *dma_alloc(size_t size, size_t align, uint64_t *phys_out) {
    if (size == 0) return NULL;
    if (align < 64) align = 64;
    void *phys_ptr = pmm_alloc_dma(size, align);
    if (!phys_ptr) return NULL;
    uint64_t phys = (uint64_t)phys_ptr;
    void *virt = (void *)(phys + pmm_hhdm_offset);
    if (phys_out) *phys_out = phys;
    vmm_map_dma((void *)phys, size);
    return virt;
}

void dma_free(void *ptr, size_t size) {
    if (!ptr) return;
    uint64_t phys = (uint64_t)ptr - pmm_hhdm_offset;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < pages; ++i) {
        pmm_free_page((void *)(phys + i * PAGE_SIZE));
    }
}

