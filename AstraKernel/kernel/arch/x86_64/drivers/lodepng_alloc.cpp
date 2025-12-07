/* Wrapper for lodepng allocators - provides malloc/free/realloc for C++ code */
#include "types.h"
#include "kernel.h"

/* Declare kmalloc functions - they have C linkage already */
extern "C" {
void* kmalloc(size_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t new_size);
}

/* Provide standard malloc/free/realloc - must match C++ name mangling */
void* malloc(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr && size > 0) {
        printf("lodepng_alloc: malloc(%zu) failed!\n", size);
    }
    return ptr;
}

void free(void* ptr) {
    if (ptr) kfree(ptr);
}

void* realloc(void* ptr, size_t new_size) {
    void* new_ptr = krealloc(ptr, new_size);
    if (!new_ptr && new_size > 0) {
        printf("lodepng_alloc: realloc(%p, %zu) failed!\n", ptr, new_size);
    }
    return new_ptr;
}

/* Also provide lodepng-specific allocators - C++ linkage to match lodepng.cpp */
void* lodepng_malloc(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr && size > 0) {
        printf("lodepng_alloc: lodepng_malloc(%zu) failed!\n", size);
    }
    return ptr;
}

void lodepng_free(void* ptr) {
    if (ptr) kfree(ptr);
}

void* lodepng_realloc(void* ptr, size_t new_size) {
    void* new_ptr = krealloc(ptr, new_size);
    if (!new_ptr && new_size > 0) {
        printf("lodepng_alloc: lodepng_realloc(%p, %zu) failed!\n", ptr, new_size);
    }
    return new_ptr;
}

