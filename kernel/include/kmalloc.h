#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void *kmalloc(size_t sz);
void kfree(void *ptr);
void *kmemalign(size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif
