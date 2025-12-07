#pragma once

#include "types.h"
#include "mm/metadata.hpp"
#include "pmm.h"

namespace mm {

class BuddyAllocator {
public:
    void init(uint64_t heap_phys_base, size_t heap_bytes);
    void *allocate(size_t size, size_t align);
    void deallocate(void *ptr, size_t size);

private:
    static constexpr int MAX_ORDER = 20; // up to ~4GiB span
    struct free_block {
        free_block *next;
        free_block *prev;
    };

    uint64_t base_;
    size_t   length_;
    free_block *free_lists_[MAX_ORDER+1];

    int order_for_size(size_t size, size_t align) const;
    uint64_t addr_to_index(uint64_t addr) const;
    void push_block(int order, free_block *blk);
    free_block *pop_block(int order);
    void split_until(int order, int target);
};

} // namespace mm


