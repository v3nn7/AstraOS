#pragma once

#include "types.h"

#define PAGE_SIZE 4096ULL
#define KERNEL_BASE 0xFFFFFFFF80000000ULL

#define PAGE_PRESENT        (1ULL << 0)
#define PAGE_WRITE          (1ULL << 1)
#define PAGE_USER           (1ULL << 2)
#define PAGE_WRITE_THROUGH  (1ULL << 3)
#define PAGE_CACHE_DISABLE  (1ULL << 4)
#define PAGE_HUGE           (1ULL << 7)
#define PAGE_GLOBAL         (1ULL << 8)

void paging_init(phys_addr_t kernel_start, phys_addr_t kernel_end);
int map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags);
void unmap_page(virt_addr_t virt);
