#pragma once

#include <stdint.h>
#include "pmm.h"
#include "memory.h"

#define PAGE_PRESENT       (1u << 0)
#define PAGE_WRITE         (1u << 1)
#define PAGE_CACHE_DISABLE (1u << 4)

static inline phys_addr_t vmm_virt_to_phys(uint64_t virt) {
    return (phys_addr_t)virt;
}

static inline int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    (void)virt; (void)phys; (void)flags;
    return 0;
}

