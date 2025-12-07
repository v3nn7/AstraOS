#pragma once

#include "types.h"
#include "mm/metadata.hpp"
#include "pmm.h"

namespace mm {

class DmaAllocator {
public:
    void init();
    void *allocate(size_t size, size_t align);
    void deallocate(void *ptr, size_t size);

private:
    struct free_block {
        free_block *next;
        size_t size;
    };

    free_block *free_list_;
    void add_region(void *addr, size_t size);
};

} // namespace mm


