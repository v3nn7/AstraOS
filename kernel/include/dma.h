#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AstraOS DMA Memory API
 * ----------------------
 * All DMA memory returned by this API is:
 *   - physically contiguous
 *   - aligned to at least 64 bytes (xHCI requirement)
 *   - allocated below 4GB (for 32-bit PCI DMA compatibility)
 *   - mapped as uncacheable (PWT | PCD)
 *
 * These routines are used by PCI drivers such as:
 *   - xHCI (USB3)
 *   - AHCI (SATA)
 *   - NVMe
 *   - network cards
 *   - any device requiring bus-master DMA access
 */

/* Allocate DMA memory.
 *
 * Arguments:
 *   size      – required allocation size in bytes
 *   align     – alignment (will be rounded to >= 64 bytes)
 *   phys_out  – optional pointer to receive physical address
 *
 * Returns:
 *   A virtual address mapped in the kernel's HHDM or VA space.
 */
void* dma_alloc(size_t size, size_t align, phys_addr_t* phys_out);

/* Free previously allocated DMA memory.
 * `size` must match the original allocation length. */
void dma_free(void* virt, size_t size);

/* Convert virtual address → physical address.
 * Automatically handles HHDM mappings. */
phys_addr_t virt_to_phys(void* virt);

/* Convert physical address → virtual HHDM address. */
void* phys_to_virt(phys_addr_t phys);

/* Map a single 4KB page as DMA (uncached).
 * Used internally by the allocator and by device drivers. */
void paging_map_dma(uintptr_t virt_addr, phys_addr_t phys_addr);

#ifdef __cplusplus
}
#endif
