#include <kmalloc.h>
#include <string.h>

/* Prosty bump allocator na potrzeby wczesnego etapu kernel. */
static unsigned char kmalloc_buffer[1024 * 1024];
static size_t kmalloc_offset = 0;

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    /* Wyrównanie do 8 bajtów */
    size = (size + 7) & ~((size_t)7);
    if (kmalloc_offset + size > sizeof(kmalloc_buffer)) {
        return NULL;
    }
    void *ptr = &kmalloc_buffer[kmalloc_offset];
    kmalloc_offset += size;
    return ptr;
}

void kfree(void *ptr) {
    (void)ptr; /* No-op w bump allocatorze */
}

