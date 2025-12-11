#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate physically contiguous, page-aligned, <4GB DMA memory. */
void *dma_alloc(size_t size, size_t align, phys_addr_t *phys_out);
void dma_free(void *virt, size_t size);

#ifdef __cplusplus
}
#endif

