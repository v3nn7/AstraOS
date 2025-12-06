#pragma once

#include "types.h"
#include "interrupts.h"

#define VMM_FLAGS_DEFAULT   (PAGE_PRESENT | PAGE_WRITE)
#define VMM_FLAGS_DEVICE    (PAGE_PRESENT | PAGE_WRITE | PAGE_CACHE_DISABLE)

void vmm_init(void);
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t virt);
void *vmm_map_dma(void *phys, size_t size);

void vmm_page_fault_handler(interrupt_frame_t *frame, uint64_t code);

