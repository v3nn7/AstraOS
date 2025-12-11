#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void vmm_init();
void vmm_init_with_map(uintptr_t mem_map, size_t map_size, size_t desc_size);
void vmm_map(uintptr_t virt, uintptr_t phys, uint32_t flags);
void vmm_unmap(uintptr_t virt);
uintptr_t vmm_translate(uintptr_t virt);
uintptr_t vmm_map_mmio(uintptr_t phys_addr, size_t size);
uintptr_t vmm_alloc_region(size_t size);

#ifdef __cplusplus
}
#endif
