#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.h"

namespace PMM {

    void init(uintptr_t memory_map, size_t memory_map_size, size_t desc_size);

    void* alloc_page();

    void* alloc_contiguous(size_t pages, size_t align, uint64_t max_phys, phys_addr_t* phys_out);

    void free_page(void* addr);

    uint64_t total_memory();

    uint64_t free_memory();

}