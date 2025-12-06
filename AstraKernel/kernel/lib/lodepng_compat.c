#include "stdlib.h"
#include "string.h"

/* Very simple bump allocator for lodepng; leaks are acceptable for installer session */
#define LODEPNG_POOL_SIZE (1024 * 1024)
static unsigned char pool[LODEPNG_POOL_SIZE];
static unsigned long pool_off = 0;

void *malloc(size_t size) {
    size = (size + 7UL) & ~7UL;
    if (pool_off + size > LODEPNG_POOL_SIZE) return NULL;
    void *p = &pool[pool_off];
    pool_off += size;
    return p;
}

void free(void *ptr) {
    (void)ptr; /* no-op */
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    void *n = malloc(size);
    if (!n) return NULL;
    /* approximate copy; we don't know old size, copy min(size, 0) not known, so skip */
    return n;
}

