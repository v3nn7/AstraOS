#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum heap_block_tag_e {
    HEAP_TAG_SLAB  = 0,
    HEAP_TAG_BUDDY = 1,
    HEAP_TAG_DMA   = 2,
    HEAP_TAG_SAFE  = 3
} heap_block_tag_t;

void heap_cpp_init(void);
void *heap_alloc(size_t size, size_t align, heap_block_tag_t tag);
void heap_free(void *ptr);
void *heap_realloc(void *ptr, size_t new_size);
void heap_stats(void);

#ifdef __cplusplus
}
#endif

