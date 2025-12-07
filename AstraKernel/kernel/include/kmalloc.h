#pragma once

#include "types.h"

void kmalloc_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);
void *kmalloc_perf(size_t size);
void *kmalloc_dma(size_t size);
void *kmalloc_safe(size_t size);

