#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Higher-half direct map offset (set by physical memory manager). */
#ifdef __cplusplus
extern "C" {
#endif
extern uint64_t pmm_hhdm_offset;
#ifdef __cplusplus
}
#endif

/* Initialize PMM from firmware memory map. */
void pmm_init(uintptr_t memory_map, size_t memory_map_size, size_t desc_size);

/* Allocate / free one 4KB physical page. */
void* pmm_alloc_page(void);
void  pmm_free_page(void* addr);

/* Memory stats. */
uint64_t pmm_total_memory(void);
uint64_t pmm_free_memory(void);

#ifdef __cplusplus
}
#endif
