#include "mm/buddy.hpp"
#include "string.h"
#include "kernel.h"
#include "memory.h"

using namespace mm;

static inline size_t order_span_bytes(int order) {
    return (size_t)PAGE_SIZE << order;
}

static inline uint64_t align_down_u64(uint64_t v, uint64_t a) { return v & ~(a - 1); }

void BuddyAllocator::init(uint64_t heap_phys_base, size_t heap_bytes) {
    base_ = align_down_u64(heap_phys_base, PAGE_SIZE);
    length_ = align_down_u64(heap_bytes, PAGE_SIZE);
    for (int i = 0; i <= MAX_ORDER; ++i) free_lists_[i] = nullptr;

    uint64_t cursor = base_;
    size_t remaining = length_;
    while (remaining >= PAGE_SIZE) {
        int o = 0;
        size_t span = PAGE_SIZE;
        while (o < MAX_ORDER && (span << 1) <= remaining && ((cursor - base_) % (span << 1) == 0)) {
            span <<= 1;
            ++o;
        }
        free_block *blk = (free_block *)(cursor + pmm_hhdm_offset);
        blk->next = blk->prev = nullptr;
        push_block(o, blk);
        cursor += span;
        remaining -= span;
    }
    printf("buddy: init base=%p size=%llu\n", (void *)base_, (unsigned long long)length_);
}

int BuddyAllocator::order_for_size(size_t size, size_t align) const {
    size_t need = size;
    if (align < 16) align = 16;
    need = ((need + align - 1) / align) * align;
    size_t block = 4096;
    for (int o = 0; o <= MAX_ORDER; ++o) {
        if (block >= need) return o;
        block <<= 1;
    }
    return -1;
}

void BuddyAllocator::push_block(int order, free_block *blk) {
    blk->prev = nullptr;
    blk->next = free_lists_[order];
    if (free_lists_[order]) free_lists_[order]->prev = blk;
    free_lists_[order] = blk;
}

BuddyAllocator::free_block *BuddyAllocator::pop_block(int order) {
    free_block *b = free_lists_[order];
    if (!b) return nullptr;
    free_lists_[order] = b->next;
    if (b->next) b->next->prev = nullptr;
    b->next = b->prev = nullptr;
    return b;
}

void BuddyAllocator::split_until(int order, int target) {
    if (order <= target) return;
    free_block *b = pop_block(order);
    if (!b) return;
    size_t size = 4096ULL << order;
    size_t half = size >> 1;
    uint64_t addr = (uint64_t)b - pmm_hhdm_offset;
    free_block *b0 = (free_block *)(addr + pmm_hhdm_offset);
    free_block *b1 = (free_block *)(addr + half + pmm_hhdm_offset);
    push_block(order - 1, b0);
    push_block(order - 1, b1);
    split_until(order - 1, target);
}

uint64_t BuddyAllocator::addr_to_index(uint64_t addr) const {
    return (addr - base_) / 4096;
}

void *BuddyAllocator::allocate(size_t size, size_t align) {
    int ord = order_for_size(size, align);
    if (ord < 0) return nullptr;
    int o = ord;
    for (; o <= MAX_ORDER; ++o) {
        if (free_lists_[o]) break;
    }
    if (o > MAX_ORDER) return nullptr;
    split_until(o, ord);
    free_block *b = pop_block(ord);
    if (!b) return nullptr;
    uint64_t virt = (uint64_t)b;
    uint64_t phys = virt - pmm_hhdm_offset;
    
    /* Validate: virt must be in HHDM range, phys must be in buddy region */
    if (virt < pmm_hhdm_offset || virt >= pmm_hhdm_offset + pmm_max_physical) {
        printf("buddy: alloc returned invalid HHDM address virt=%p\n", (void *)virt);
        return nullptr;
    }
    if (phys < base_ || phys >= base_ + length_) {
        printf("buddy: alloc returned address outside region phys=%p base=%p len=%zu\n",
               (void *)phys, (void *)base_, length_);
        return nullptr;
    }
    
    printf("buddy: alloc size=%zu align=%zu order=%d phys=%p virt=%p\n",
           size, align, ord, (void *)phys, (void *)virt);
    return (void *)virt;
}

void BuddyAllocator::deallocate(void *ptr, size_t size) {
    if (!ptr) return;
    int ord = order_for_size(size, 16);
    if (ord < 0) {
        printf("buddy: invalid free size=%zu ptr=%p\n", size, ptr);
        return;
    }
    uint64_t addr = (uint64_t)ptr - pmm_hhdm_offset;
    if (addr < base_ || addr >= base_ + length_) {
        printf("buddy: free out of range ptr=%p phys=%p\n", ptr, (void *)addr);
        return;
    }
    free_block *b = (free_block *)(addr + pmm_hhdm_offset);
    push_block(ord, b);
    // TODO: optional lazy merge
}


