#pragma once
#include <stdint.h>
#include <stddef.h>

namespace Paging {

    constexpr uint64_t PAGE_SIZE = 4096;

    enum Flags : uint64_t {
        PRESENT        = 1ull << 0,
        WRITABLE       = 1ull << 1,
        USER           = 1ull << 2,
        WRITE_THROUGH  = 1ull << 3,
        NO_CACHE       = 1ull << 4,
        ACCESSED       = 1ull << 5,
        DIRTY          = 1ull << 6,
        HUGE_2MB       = 1ull << 7,  // Leaf bit 7 in PD entry
        GLOBAL         = 1ull << 8,
        HUGE_1GB       = 1ull << 9
    };

    struct PageTable {
        uint64_t entry[512];
    } __attribute__((aligned(4096)));

    /* Initialize paging and enable CR3 / CR0.PG / CR4.PAE */
    void init();

    /* Map 4KB page or hugepage */
    void map(uint64_t virt, uint64_t phys, uint64_t flags);

    /* Unmap a virtual address */
    void unmap(uint64_t virt);

    /* Translate virtual address → physical address (0 = unmapped) */
    uint64_t translate(uint64_t virt);

    /* Map a physical MMIO region (uncached) */
    uint64_t map_mmio(uint64_t phys, size_t size);

    /* Allocate a new virtual region without mapping */
    uint64_t alloc_virt_region(size_t size);

    extern PageTable* pml4;
}
