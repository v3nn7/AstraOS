#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Flags (bitmask) */
#define VMM_PRESENT        (1u << 0)
#define VMM_WRITABLE       (1u << 1)
#define VMM_USER           (1u << 2)
#define VMM_WRITE_THROUGH  (1u << 3)
#define VMM_NO_CACHE       (1u << 4)
#define VMM_HUGE_2MB       (1u << 5)
#define VMM_HUGE_1GB       (1u << 6)

/* Initialize the full virtual memory manager. */
void vmm_init();

/* Map a single page (4KB or huge page). */
void vmm_map(uintptr_t virt, uintptr_t phys, uint32_t flags);

/* Unmap a page. */
void vmm_unmap(uintptr_t virt);

/* Translate virtual → physical (returns 0 if not mapped). */
uintptr_t vmm_translate(uintptr_t virt);

/* MMIO mapping: map a physical device range as uncached. */
uintptr_t vmm_map_mmio(uintptr_t phys_addr, size_t size);

/* Allocate new virtual region (no mapping) – optional convenience. */
uintptr_t vmm_alloc_region(size_t size);

#ifdef __cplusplus
}
#endif
