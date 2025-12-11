#pragma once
#include <stdint.h>
#include <stddef.h>

namespace VMM {

    constexpr uint64_t PAGE_SIZE = 4096;

    /* Masks & helpers */
    inline constexpr uint64_t PAGE_MASK = ~(PAGE_SIZE - 1);

    enum Flags : uint64_t {
        PRESENT       = 1ull << 0,
        WRITABLE      = 1ull << 1,
        USER          = 1ull << 2,
        WRITE_THROUGH = 1ull << 3,
        NO_CACHE      = 1ull << 4,
        HUGE_2MB      = 1ull << 5,
        HUGE_1GB      = 1ull << 6,
    };

    struct PageTable {
        uint64_t entry[512];
    } __attribute__((aligned(4096)));

    void init(uintptr_t mem_map, size_t map_size, size_t desc_size);
    void map(uint64_t virt, uint64_t phys, uint64_t flags);
    void unmap(uint64_t virt);
    uint64_t translate(uint64_t virt);

    uintptr_t map_mmio(uintptr_t phys, size_t size);

    uintptr_t alloc_virt_region(size_t size);

    extern PageTable* pml4;
}
