#include "kmalloc.h"
#include "heap_api.h"
#include "memory.h"
#include "string.h"
#include "kernel.h"
#include "stddef.h"

void kmalloc_init(void) {
    printf("kmalloc: initializing C++ heap\n");
    heap_cpp_init();
    printf("kmalloc: C++ heap initialized\n");
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    heap_block_tag_t tag = (size <= 1024) ? HEAP_TAG_SLAB : HEAP_TAG_BUDDY;
    return heap_alloc(size, 16, tag);
}

void *kcalloc(size_t n, size_t size) {
    size_t total = n * size;
    void *ptr = kmalloc(total);
    if (ptr) k_memset(ptr, 0, total);
    return ptr;
}

void *krealloc(void *ptr, size_t size) {
    return heap_realloc(ptr, size);
}

void kfree(void *ptr) {
    heap_free(ptr);
}

void *kmalloc_perf(size_t size) { return heap_alloc(size, 16, HEAP_TAG_BUDDY); }
void *kmalloc_dma(size_t size) { return heap_alloc(size, 64, HEAP_TAG_DMA); }
void *kmalloc_safe(size_t size) { return heap_alloc(size, 16, HEAP_TAG_SAFE); }

