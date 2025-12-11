#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal virtual memory manager placeholders for build compatibility. */
static inline uintptr_t vmm_map_mmio(uintptr_t phys_addr, size_t size) {
    (void)size;
    return phys_addr;
}

static inline void vmm_map(uintptr_t virt, uintptr_t phys, uint32_t flags) {
    (void)virt;
    (void)phys;
    (void)flags;
}

#ifdef __cplusplus
}
#endif
