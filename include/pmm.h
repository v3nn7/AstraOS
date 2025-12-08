#pragma once

#include <stdint.h>

typedef uint64_t phys_addr_t;

static inline phys_addr_t pmm_alloc_page(void) { return 0; }
static inline void pmm_free_page(phys_addr_t addr) { (void)addr; }

extern uint64_t pmm_hhdm_offset;

