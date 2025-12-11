#pragma once
#include <stdint.h>
#include <stddef.h>

namespace PMM {

    /* Initialize PMM from the EFI memory map passed by bootloader */
    void init(uintptr_t memory_map, size_t memory_map_size, size_t desc_size);

    /* Allocate one 4KB physical page */
    void* alloc_page();

    /* Free one physical page */
    void free_page(void* addr);

    /* Get total usable memory (bytes) */
    uint64_t total_memory();

    /* Get free memory (bytes) */
    uint64_t free_memory();

}
