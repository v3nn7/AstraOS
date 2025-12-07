#pragma once

#include "types.h"
#include "mm/metadata.hpp"
#include "pmm.h"

namespace mm {

static constexpr uint32_t SLAB_MAGIC = 0x534C4142; /* 'SLAB' */
static constexpr uint32_t SLAB_FREELIST_MAGIC = 0xFEE1DEAD;

struct slab_page {
    uint32_t magic;
    uint32_t freelist_magic;
    slab_page *next;
    uint16_t free_count;
    uint16_t class_size;
    uint64_t phys_base;
    uint8_t *freelist;
};

class SlabAllocator {
public:
    SlabAllocator();
    void init();
    void *allocate(size_t size, size_t align);
    void deallocate(void *ptr);

private:
    static constexpr int CLASS_COUNT = 9; // 16,32,64,128,256,512,1024,1536,2048
    slab_page *classes_[CLASS_COUNT];
    bool validate_page(slab_page *page, int expect_idx) const;
    bool validate_ptr(void *ptr, slab_page **out_page, int *out_idx) const;
    size_t class_size_for_idx(int idx) const;
    int class_index(size_t size) const;
    slab_page *new_page(size_t class_size);
    void *alloc_from_page(slab_page *page);
};

} // namespace mm


