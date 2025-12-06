#include "kmalloc.h"
#include "pmm.h"
#include "memory.h"
#include "string.h"
#include "kernel.h"
#include "stddef.h"

#define KMALLOC_MAGIC 0xC0FFEE01u

typedef struct alloc_header {
    uint32_t magic;
    uint32_t bucket;
    uint64_t size;
    uint64_t pages;
} alloc_header_t;

static const size_t bucket_sizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096};
static void *free_lists[sizeof(bucket_sizes) / sizeof(bucket_sizes[0])] = {0};

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
static inline uint64_t phys_of(void *virt) { return (uint64_t)virt - pmm_hhdm_offset; }

static inline alloc_header_t *hdr_from_user(void *ptr) {
    return ((alloc_header_t *)ptr) - 1;
}

static int bucket_for_size(size_t size) {
    for (size_t i = 0; i < sizeof(bucket_sizes) / sizeof(bucket_sizes[0]); ++i) {
        if (size <= bucket_sizes[i] - sizeof(alloc_header_t)) return (int)i;
    }
    return -1;
}

static void push_free(int bucket, void *user_ptr) {
    *(void **)user_ptr = free_lists[bucket];
    free_lists[bucket] = user_ptr;
}

static void *pop_free(int bucket) {
    void *ptr = free_lists[bucket];
    if (ptr) {
        free_lists[bucket] = *(void **)ptr;
    }
    return ptr;
}

static void refill_bucket(int bucket) {
    uint64_t phys = (uint64_t)pmm_alloc_page();
    if (!phys) {
        printf("kmalloc: out of pages for bucket %d\n", bucket);
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    uint8_t *page = (uint8_t *)(phys + pmm_hhdm_offset);
    size_t bsize = bucket_sizes[bucket];
    size_t blocks = PAGE_SIZE / bsize;
    for (size_t i = 0; i < blocks; ++i) {
        alloc_header_t *h = (alloc_header_t *)(page + i * bsize);
        h->magic = KMALLOC_MAGIC;
        h->bucket = (uint32_t)bucket;
        h->size = bsize - sizeof(alloc_header_t);
        h->pages = 1;
        void *user = (void *)(h + 1);
        push_free(bucket, user);
    }
}

void kmalloc_init(void) {
    for (size_t i = 0; i < sizeof(free_lists) / sizeof(free_lists[0]); ++i) {
        free_lists[i] = NULL;
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    int bucket = bucket_for_size(size);
    if (bucket >= 0) {
        if (!free_lists[bucket]) {
            refill_bucket(bucket);
        }
        void *user = pop_free(bucket);
        if (!user) {
            printf("kmalloc: failed to pop bucket %d\n", bucket);
            return NULL;
        }
        alloc_header_t *h = hdr_from_user(user);
        h->size = size;
        return user;
    }

    size_t total = size + sizeof(alloc_header_t);
    size_t pages = align_up_u64(total, PAGE_SIZE) / PAGE_SIZE;
    uint64_t phys = (uint64_t)pmm_alloc_pages(pages);
    if (!phys) {
        printf("kmalloc: large alloc failed size=%llu\n", (unsigned long long)size);
        return NULL;
    }
    alloc_header_t *h = (alloc_header_t *)(phys + pmm_hhdm_offset);
    h->magic = KMALLOC_MAGIC;
    h->bucket = 0xFFFFFFFFu;
    h->size = size;
    h->pages = pages;
    return (void *)(h + 1);
}

void *kcalloc(size_t n, size_t size) {
    size_t total = n * size;
    void *ptr = kmalloc(total);
    if (ptr) k_memset(ptr, 0, total);
    return ptr;
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    alloc_header_t *h = hdr_from_user(ptr);
    if (h->magic != KMALLOC_MAGIC) {
        printf("krealloc: invalid pointer %p\n", ptr);
        return NULL;
    }

    if (size <= h->size) return ptr;

    void *n = kmalloc(size);
    if (!n) return NULL;
    memcpy(n, ptr, h->size);
    kfree(ptr);
    return n;
}

void kfree(void *ptr) {
    if (!ptr) return;
    alloc_header_t *h = hdr_from_user(ptr);
    if (h->magic != KMALLOC_MAGIC) {
        printf("kfree: invalid pointer %p\n", ptr);
        return;
    }

    if (h->bucket != 0xFFFFFFFFu) {
        push_free((int)h->bucket, ptr);
        return;
    }

    /* Large allocation */
    uint64_t phys = phys_of(h);
    for (uint64_t i = 0; i < h->pages; ++i) {
        pmm_free_page((void *)(phys + i * PAGE_SIZE));
    }
}

