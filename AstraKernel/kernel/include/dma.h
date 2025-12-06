#pragma once

#include "types.h"

void dma_init(void);
void *dma_alloc(size_t size, size_t align, uint64_t *phys_out);
void dma_free(void *ptr, size_t size);

