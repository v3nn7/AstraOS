#include "pmm.hpp"

namespace {

    constexpr uint64_t PAGE_SIZE = 4096;

    static uint8_t* bitmap = nullptr;
    static size_t bitmap_size = 0;
    static uint64_t usable_mem = 0;
    static uint64_t total_pages = 0;

    inline void bitmap_set(size_t bit)   { bitmap[bit / 8] |=  (1 << (bit % 8)); }
    inline void bitmap_clear(size_t bit) { bitmap[bit / 8] &= ~(1 << (bit % 8)); }
    inline bool bitmap_used(size_t bit)  { return bitmap[bit / 8] &   (1 << (bit % 8)); }

}

/* ------------------------------------------------------------------------- */
/*            Initialize PMM using UEFI memory map                            */
/* ------------------------------------------------------------------------- */

void PMM::init(uintptr_t mem_map, size_t map_size, size_t desc_size) {

    /* Count total usable pages */
    usable_mem = 0;
    total_pages = 0;

    for (size_t off = 0; off < map_size; off += desc_size) {

        struct {
            uint32_t type;
            uint32_t pad;
            uint64_t phys;
            uint64_t virt;
            uint64_t pages;
            uint64_t attrs;
        } *d = (decltype(d))(mem_map + off);

        /* EfiConventionalMemory = type 7 (usable RAM) */
        if (d->type == 7) {
            usable_mem += d->pages * PAGE_SIZE;
            total_pages += d->pages;
        }
    }

    /* Allocate bitmap — 1 bit per page */
    bitmap_size = (total_pages + 7) / 8;

    // put bitmap right after kernel (HIGH VA)
    bitmap = (uint8_t*)0xffff800000000000; // możesz zmienić
    for (size_t i = 0; i < bitmap_size; i++) bitmap[i] = 0;

    /* Mark all pages from memory map */
    size_t bit_index = 0;

    for (size_t off = 0; off < map_size; off += desc_size) {

        struct {
            uint32_t type;
            uint32_t pad;
            uint64_t phys;
            uint64_t virt;
            uint64_t pages;
            uint64_t attrs;
        } *d = (decltype(d))(mem_map + off);

        for (uint64_t i = 0; i < d->pages; i++) {

            if (d->type == 7) {
                bitmap_clear(bit_index);   // free
            } else {
                bitmap_set(bit_index);     // reserved
            }
            bit_index++;
        }
    }
}

/* ------------------------------------------------------------------------- */
/*                             Allocation                                     */
/* ------------------------------------------------------------------------- */

void* PMM::alloc_page() {
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_used(i)) {
            bitmap_set(i);
            return (void*)(i * PAGE_SIZE);
        }
    }
    return nullptr; // out of memory
}

void PMM::free_page(void* addr) {
    size_t index = (uintptr_t)addr / PAGE_SIZE;
    bitmap_clear(index);
}

/* ------------------------------------------------------------------------- */
/*                             Info API                                       */
/* ------------------------------------------------------------------------- */

uint64_t PMM::total_memory() {
    return usable_mem;
}

uint64_t PMM::free_memory() {
    uint64_t free = 0;
    for (size_t i = 0; i < total_pages; i++)
        if (!bitmap_used(i)) free += PAGE_SIZE;
    return free;
}

/* ------------------------------------------------------------------------- */
/*                      C wrappers for C codebase                           */
/* ------------------------------------------------------------------------- */
extern "C" {
    void pmm_init(uintptr_t memory_map, size_t memory_map_size, size_t desc_size) {
        PMM::init(memory_map, memory_map_size, desc_size);
    }

    void* pmm_alloc_page(void) {
        return PMM::alloc_page();
    }

    void pmm_free_page(void* addr) {
        PMM::free_page(addr);
    }

    uint64_t pmm_total_memory(void) {
        return PMM::total_memory();
    }

    uint64_t pmm_free_memory(void) {
        return PMM::free_memory();
    }
}
