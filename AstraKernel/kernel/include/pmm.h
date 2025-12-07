#pragma once

#include "types.h"
#include "limine.h"

#ifdef __cplusplus
extern "C" {
#endif

void pmm_init(struct limine_memmap_response *mmap);
void *pmm_alloc_page(void);
void pmm_free_page(void *p);
void *pmm_alloc_pages(size_t count);
void *pmm_alloc_dma(size_t size, size_t align);

extern uint64_t pmm_hhdm_offset;
extern uint64_t pmm_max_physical;

#ifdef __cplusplus
}
#endif

