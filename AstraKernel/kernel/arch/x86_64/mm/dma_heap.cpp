#include "mm/dma_heap.hpp"
#include "string.h"

using namespace mm;

void DmaAllocator::init() {
    free_list_ = nullptr;
    const int pages = 16;
    void *phys = pmm_alloc_pages(pages);
    if (!phys) return;
    add_region((void *)((uint64_t)phys + pmm_hhdm_offset), pages * 4096);
}

void DmaAllocator::add_region(void *addr, size_t size) {
    free_block *blk = (free_block *)addr;
    blk->size = size;
    blk->next = free_list_;
    free_list_ = blk;
}

void *DmaAllocator::allocate(size_t size, size_t align) {
    if (align < 64) align = 64;
    size_t need = (size + align - 1) & ~(align - 1);
    free_block *prev = nullptr;
    free_block *cur = free_list_;
    while (cur) {
        uintptr_t base = (uintptr_t)cur;
        uintptr_t aligned = (base + align - 1) & ~(uintptr_t)(align - 1);
        size_t padding = aligned - base;
        if (cur->size >= need + padding) {
            if (padding) {
                free_block *head = cur;
                head->size = padding;
                free_block *rest = (free_block *)(aligned);
                rest->size = cur->size - padding;
                rest->next = cur->next;
                if (prev) prev->next = head;
                else free_list_ = head;
                cur = rest;
            }
            if (cur->size > need + 128) {
                free_block *rest = (free_block *)((uint8_t *)cur + need);
                rest->size = cur->size - need;
                rest->next = cur->next;
                if (prev) prev->next = rest;
                else free_list_ = rest;
            } else {
                if (prev) prev->next = cur->next;
                else free_list_ = cur->next;
            }
            return (void *)aligned;
        }
        prev = cur;
        cur = cur->next;
    }
    return nullptr;
}

void DmaAllocator::deallocate(void *ptr, size_t size) {
    if (!ptr) return;
    free_block *blk = (free_block *)ptr;
    blk->size = size;
    blk->next = free_list_;
    free_list_ = blk;
}


