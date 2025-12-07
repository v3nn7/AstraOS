#include "pmm.h"
#include "memory.h"
#include "string.h"
#include "kernel.h"
#include "stddef.h"

extern volatile struct limine_hhdm_request limine_hhdm_request;
extern volatile struct limine_executable_address_request limine_exec_addr_request;
extern char _kernel_start[];
extern char _kernel_end[];

uint64_t pmm_hhdm_offset = 0;
uint64_t pmm_max_physical = 0;

static uint8_t *bitmap = NULL;
static size_t bitmap_bytes = 0;
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

static inline uint64_t align_up_u64(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }
static inline uint64_t align_down_u64(uint64_t v, uint64_t a) { return v & ~(a - 1); }
static inline void *phys_to_virt(uint64_t phys) { return (void *)(phys + pmm_hhdm_offset); }

static inline void bitmap_set(size_t idx) { bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7)); }
static inline void bitmap_clear(size_t idx) { bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7)); }
static inline int bitmap_test(size_t idx) { return (bitmap[idx >> 3] >> (idx & 7)) & 1; }

static void mark_range(size_t start_page, size_t page_count, int used) {
    for (size_t i = 0; i < page_count; ++i) {
        size_t idx = start_page + i;
        if (used) {
            if (!bitmap_test(idx)) {
                bitmap_set(idx);
                if (free_pages) --free_pages;
            }
        } else {
            if (bitmap_test(idx)) {
                bitmap_clear(idx);
                ++free_pages;
            }
        }
    }
}

static ssize_t find_free_run_bounded(size_t pages, size_t align_pages, size_t max_page) {
    if (pages == 0 || total_pages == 0) return -1;
    if (align_pages == 0) align_pages = 1;
    size_t limit = (max_page && max_page < total_pages) ? max_page : total_pages;
    if (limit < pages) return -1;
    limit -= pages;
    for (size_t i = 0; i <= limit; ) {
        size_t aligned = (i + align_pages - 1) & ~(align_pages - 1);
        if (aligned != i) {
            i = aligned;
            continue;
        }
        size_t j = 0;
        for (; j < pages; ++j) {
            if (bitmap_test(i + j)) {
                i += j + 1;
                break;
            }
        }
        if (j == pages) return (ssize_t)i;
    }
    return -1;
}

static inline ssize_t find_free_run(size_t pages, size_t align_pages) {
    return find_free_run_bounded(pages, align_pages, 0);
}

void pmm_init(struct limine_memmap_response *mmap) {
    if (!mmap || mmap->entry_count == 0) {
        printf("PMM: invalid memory map\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    if (!limine_hhdm_request.response) {
        printf("PMM: missing HHDM response\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    pmm_hhdm_offset = limine_hhdm_request.response->offset;

    pmm_max_physical = 0;
    for (uint64_t i = 0; i < mmap->entry_count; ++i) {
        struct limine_memmap_entry *e = mmap->entries[i];
        uint64_t end = e->base + e->length;
        if (end > pmm_max_physical) pmm_max_physical = end;
    }
    total_pages = align_up_u64(pmm_max_physical, PAGE_SIZE) / PAGE_SIZE;
    bitmap_bytes = align_up_u64((total_pages + 7) / 8, PAGE_SIZE);

    /* Get real kernel physical address from Limine */
    struct limine_executable_address_response *addr = limine_exec_addr_request.response;
    uint64_t kernel_phys_base = addr ? addr->physical_base : 0x100000;
    uint64_t kernel_size = (uint64_t)_kernel_end - (uint64_t)_kernel_start;
    uint64_t kernel_phys_end = kernel_phys_base + kernel_size;
    
    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < mmap->entry_count; ++i) {
        struct limine_memmap_entry *e = mmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;
        uint64_t start = align_up_u64(e->base, PAGE_SIZE);
        uint64_t end   = e->base + e->length;
        if (start < kernel_phys_end) {
            start = align_up_u64(kernel_phys_end, PAGE_SIZE);
        }
        if (start + bitmap_bytes <= end) {
            bitmap_phys = start;
            break;
        }
    }
    if (!bitmap_phys) {
        printf("PMM: unable to place bitmap\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }

    bitmap = (uint8_t *)phys_to_virt(bitmap_phys);
    k_memset(bitmap, 0xFF, bitmap_bytes);
    free_pages = 0;

    for (uint64_t i = 0; i < mmap->entry_count; ++i) {
        struct limine_memmap_entry *e = mmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;
        uint64_t start = align_down_u64(e->base, PAGE_SIZE);
        uint64_t end = align_up_u64(e->base + e->length, PAGE_SIZE);
        size_t first = start / PAGE_SIZE;
        size_t pages = (end - start) / PAGE_SIZE;
        mark_range(first, pages, 0);
    }

    uint64_t k_start = align_down_u64(kernel_phys_base, PAGE_SIZE);
    uint64_t k_end   = align_up_u64(kernel_phys_end, PAGE_SIZE);
    mark_range(k_start / PAGE_SIZE, (k_end - k_start) / PAGE_SIZE, 1);

    mark_range(bitmap_phys / PAGE_SIZE, bitmap_bytes / PAGE_SIZE, 1);

    printf("PMM: total=%x pages free=%x bitmap=%x bytes hhdm=%x\n",
           (uint64_t)total_pages,
           (uint64_t)free_pages,
           (uint64_t)bitmap_bytes,
           (uint64_t)pmm_hhdm_offset);
}

void *pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    ssize_t idx = find_free_run(count, 1);
    if (idx < 0) return NULL;
    mark_range((size_t)idx, count, 1);
    return (void *)((uint64_t)idx * PAGE_SIZE);
}

void *pmm_alloc_page(void) {
    return pmm_alloc_pages(1);
}

void pmm_free_page(void *p) {
    if (!p) return;
    uint64_t phys = (uint64_t)p;
    size_t idx = phys / PAGE_SIZE;
    mark_range(idx, 1, 0);
}

void *pmm_alloc_dma(size_t size, size_t align) {
    if (size == 0) return NULL;
    if (align < PAGE_SIZE) align = PAGE_SIZE;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t align_pages = align / PAGE_SIZE;
    /* Prefer below 4GiB for common DMA constraints */
    size_t dma_limit = (0x100000000ULL) / PAGE_SIZE;
    ssize_t idx = find_free_run_bounded(pages, align_pages, dma_limit);
    if (idx < 0) {
        idx = find_free_run(pages, align_pages);
    }
    if (idx < 0) return NULL;
    mark_range((size_t)idx, pages, 1);
    return (void *)((uint64_t)idx * PAGE_SIZE);
}

