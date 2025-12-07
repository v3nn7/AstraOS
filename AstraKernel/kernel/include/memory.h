#pragma once

#include "types.h"
#include "limine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE 4096ULL
#define KERNEL_BASE 0xFFFFFFFF80000000ULL

#define PAGE_PRESENT        (1ULL << 0)
#define PAGE_WRITE          (1ULL << 1)
#define PAGE_USER           (1ULL << 2)
#define PAGE_WRITE_THROUGH  (1ULL << 3)
#define PAGE_CACHE_DISABLE  (1ULL << 4)
#define PAGE_ACCESSED       (1ULL << 5)
#define PAGE_DIRTY          (1ULL << 6)
#define PAGE_HUGE           (1ULL << 7)   /* 2 MiB pages */
#define PAGE_GLOBAL         (1ULL << 8)

/* Physical Memory Manager */
void pmm_init(struct limine_memmap_response *mmap);
void *pmm_alloc_page(void);
void pmm_free_page(void *p);
void *pmm_alloc_pages(size_t count);
void *pmm_alloc_dma(size_t size, size_t align);
extern uint64_t pmm_hhdm_offset;
extern uint64_t pmm_max_physical;

/* Virtual Memory Manager */
void vmm_init(void);
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t virt);
void *vmm_map_dma(void *phys, size_t size);

/* Kernel Heap */
void kmalloc_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

/* DMA-safe allocator */
void dma_init(void);
void *dma_alloc(size_t size, size_t align, uint64_t *phys_out);
void dma_free(void *ptr, size_t size);

/* Legacy paging API (implemented as compatibility layer) */
void paging_init(phys_addr_t kernel_start, phys_addr_t kernel_end);
int map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags);
void unmap_page(virt_addr_t virt);

void memory_subsystem_init(struct limine_memmap_response *memmap);

#ifdef __cplusplus
}
#endif

