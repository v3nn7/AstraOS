#include "pmm.hpp"
#include "klog.h"

namespace {

    constexpr uint64_t PAGE_SIZE = 4096;
    constexpr size_t MAX_REGIONS = 64;

    struct Region {
        uint64_t phys_start;
        uint64_t phys_end;
        size_t page_count;
        uint8_t* bitmap;
        size_t bitmap_bytes;
    };

    static Region regions[MAX_REGIONS];
    static size_t region_count = 0;
    static uint64_t total_usable_pages = 0;

    inline void bitmap_set(uint8_t* bm, size_t bit) {
        bm[bit / 8] |= (1 << (bit % 8));
    }

    inline void bitmap_clear(uint8_t* bm, size_t bit) {
        bm[bit / 8] &= ~(1 << (bit % 8));
    }

    inline bool bitmap_used(uint8_t* bm, size_t bit) {
        return (bm[bit / 8] & (1 << (bit % 8))) != 0;
    }

    void sort_regions() {
        for (size_t i = 0; i < region_count; i++) {
            for (size_t j = i + 1; j < region_count; j++) {
                if (regions[j].phys_start < regions[i].phys_start) {
                    Region tmp = regions[i];
                    regions[i] = regions[j];
                    regions[j] = tmp;
                }
            }
        }
    }

}

extern "C" uint64_t pmm_hhdm_offset;

void PMM::init(uintptr_t mem_map, size_t map_size, size_t desc_size) {
    klog_printf(KLOG_INFO, "=== MEMORY MAP BEGIN ===");
    for (size_t off = 0; off < map_size; off += desc_size) {
        struct {
            uint32_t type;
            uint32_t pad;
            uint64_t phys;
            uint64_t virt;
            uint64_t pages;
            uint64_t attrs;
        } *d = (decltype(d))(mem_map + off);
        klog_printf(KLOG_INFO,
            "TYPE=%u PHYS=%llx PAGES=%llu ATTR=%llx",
            d->type, (unsigned long long)d->phys, (unsigned long long)d->pages, (unsigned long long)d->attrs
        );
    }
    klog_printf(KLOG_INFO, "=== MEMORY MAP END ===");

    region_count = 0;
    total_usable_pages = 0;

    for (size_t off = 0; off < map_size && region_count < MAX_REGIONS; off += desc_size) {
        struct {
            uint32_t type;
            uint32_t pad;
            uint64_t phys;
            uint64_t virt;
            uint64_t pages;
            uint64_t attrs;
        } *d = (decltype(d))(mem_map + off);

        if (d->type == 7) {
            Region* r = &regions[region_count];
            r->phys_start = d->phys;
            r->page_count = d->pages;
            r->phys_end = r->phys_start + r->page_count * PAGE_SIZE;
            r->bitmap_bytes = (r->page_count + 7) / 8;
            r->bitmap = reinterpret_cast<uint8_t*>(r->phys_start + pmm_hhdm_offset);

            for (size_t i = 0; i < r->bitmap_bytes; i++)
                r->bitmap[i] = 0xFF;

            region_count++;
            total_usable_pages += r->page_count;
        }
    }

    sort_regions();

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
            uint64_t page_phys = d->phys + i * PAGE_SIZE;

            for (size_t ri = 0; ri < region_count; ri++) {
                Region* r = &regions[ri];

                if (page_phys >= r->phys_start && page_phys < r->phys_end) {
                    size_t page_idx = (page_phys - r->phys_start) / PAGE_SIZE;
                    if (page_idx >= r->page_count) break;

                    size_t bitmap_pages = (r->bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
                    bool is_bitmap_page = (page_phys >= r->phys_start &&
                                          page_phys < r->phys_start + bitmap_pages * PAGE_SIZE);

                    if (d->type == 7 && !is_bitmap_page) {
                        bitmap_clear(r->bitmap, page_idx);
                    } else {
                        bitmap_set(r->bitmap, page_idx);
                    }
                    break;
                }
            }
        }
    }
}

void* PMM::alloc_page() {
    for (size_t ri = 0; ri < region_count; ri++) {
        Region* r = &regions[ri];
        for (size_t i = 0; i < r->page_count; i++) {
            if (!bitmap_used(r->bitmap, i)) {
                bitmap_set(r->bitmap, i);
                uint64_t phys = r->phys_start + i * PAGE_SIZE;
                return (void*)(phys + pmm_hhdm_offset);
            }
        }
    }
    return nullptr;
}

void* PMM::alloc_contiguous(size_t pages, size_t align, uint64_t max_phys, phys_addr_t* phys_out) {
    if (pages == 0) return nullptr;
    if (align < 64) align = 64;
    if (align < PAGE_SIZE) align = PAGE_SIZE;

    for (size_t ri = 0; ri < region_count; ri++) {
        Region* r = &regions[ri];
        if (r->page_count < pages) continue;

        if (max_phys > 0 && r->phys_start >= max_phys) continue;

        for (size_t start = 0; start <= r->page_count - pages; start++) {
            uint64_t phys = r->phys_start + start * PAGE_SIZE;
            uint64_t block_end = phys + pages * PAGE_SIZE;

            if (max_phys > 0 && block_end > max_phys) break;
            if ((phys % align) != 0) continue;

            bool all_free = true;
            for (size_t i = 0; i < pages; i++) {
                if (bitmap_used(r->bitmap, start + i)) {
                    all_free = false;
                    break;
                }
            }

            if (all_free) {
                for (size_t i = 0; i < pages; i++)
                    bitmap_set(r->bitmap, start + i);

                if (phys_out)
                    *phys_out = phys;

                return (void*)(phys + pmm_hhdm_offset);
            }
        }
    }
    return nullptr;
}

void PMM::free_page(void* addr) {
    uint64_t virt = (uint64_t)addr;
    uint64_t phys = virt - pmm_hhdm_offset;

    for (size_t ri = 0; ri < region_count; ri++) {
        Region* r = &regions[ri];

        if (phys >= r->phys_start && phys < r->phys_end) {
            size_t page_idx = (phys - r->phys_start) / PAGE_SIZE;
            if (page_idx < r->page_count) {
                bitmap_clear(r->bitmap, page_idx);
            }
            return;
        }
    }
}

uint64_t PMM::total_memory() {
    return total_usable_pages * PAGE_SIZE;
}

uint64_t PMM::free_memory() {
    uint64_t free = 0;
    for (size_t ri = 0; ri < region_count; ri++) {
        Region* r = &regions[ri];
        for (size_t i = 0; i < r->page_count; i++) {
            if (!bitmap_used(r->bitmap, i))
                free += PAGE_SIZE;
        }
    }
    return free;
}

extern "C" {

void pmm_init(uintptr_t memory_map, size_t memory_map_size, size_t desc_size) {
    PMM::init(memory_map, memory_map_size, desc_size);
}

void* pmm_alloc_page(void) {
    return PMM::alloc_page();
}

void* pmm_alloc_contiguous(size_t pages, size_t align) {
    return PMM::alloc_contiguous(pages, align, 0, nullptr);
}

void* pmm_alloc_contiguous_dma(size_t pages, size_t align, uint64_t max_phys, phys_addr_t* phys_out) {
    return PMM::alloc_contiguous(pages, align, max_phys, phys_out);
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