#include "mm/slab.hpp"
#include "mm/metadata.hpp"
#include "string.h"
#include "kernel.h"
#include "memory.h"

using namespace mm;

SlabAllocator::SlabAllocator() : classes_{nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr} {}

void SlabAllocator::init() {
    for (int i = 0; i < SlabAllocator::CLASS_COUNT; ++i) classes_[i] = nullptr;
}

size_t SlabAllocator::class_size_for_idx(int idx) const {
    static const size_t sizes[SlabAllocator::CLASS_COUNT] = {16,32,64,128,256,512,1024,1536,2048};
    return sizes[idx];
}

int SlabAllocator::class_index(size_t size) const {
    static const size_t sizes[SlabAllocator::CLASS_COUNT] = {16,32,64,128,256,512,1024,1536,2048};
    for (int i = 0; i < SlabAllocator::CLASS_COUNT; ++i) {
        if (size <= sizes[i]) return i;
    }
    return -1;
}

bool SlabAllocator::validate_page(slab_page *page, int expect_idx) const {
    if (!page) return false;
    if (page->magic != SLAB_MAGIC || page->freelist_magic != SLAB_FREELIST_MAGIC) return false;
    if (expect_idx >= 0 && class_index(page->class_size) != expect_idx) return false;
    if (page->class_size == 0 || page->class_size > 2048) return false;
    if (page->free_count > (PAGE_SIZE - sizeof(slab_page)) / page->class_size) return false;
    return true;
}

bool SlabAllocator::validate_ptr(void *ptr, slab_page **out_page, int *out_idx) const {
    if (!ptr) return false;
    uint64_t addr = (uint64_t)ptr;
    uint64_t page_base = addr & ~0xFFFULL;
    slab_page *hdr = (slab_page *)page_base;
    int idx = class_index(hdr->class_size);
    if (!validate_page(hdr, idx)) return false;
    /* Pointer must lie inside page body after header */
    uint64_t body_start = page_base + sizeof(slab_page);
    uint64_t body_end   = page_base + PAGE_SIZE;
    if (addr < body_start || addr >= body_end) return false;
    if (((addr - body_start) % hdr->class_size) != 0) return false;
    if (out_page) *out_page = hdr;
    if (out_idx) *out_idx = idx;
    return true;
}

slab_page *SlabAllocator::new_page(size_t class_size) {
    void *page_phys = pmm_alloc_page();
    if (!page_phys) return nullptr;
    uint8_t *page = (uint8_t *)((uint64_t)page_phys + pmm_hhdm_offset);
    k_memset(page, 0, PAGE_SIZE);

    slab_page *hdr = (slab_page *)page;
    hdr->magic = SLAB_MAGIC;
    hdr->freelist_magic = SLAB_FREELIST_MAGIC;
    hdr->next = nullptr;
    hdr->free_count = (uint16_t)((PAGE_SIZE - sizeof(slab_page)) / class_size);
    hdr->class_size = (uint16_t)class_size;
    hdr->phys_base = (uint64_t)page_phys;
    hdr->freelist = page + sizeof(slab_page);

    uint8_t *slot = hdr->freelist;
    for (uint16_t i = 0; i < hdr->free_count; ++i) {
        uint8_t *next = (i + 1 < hdr->free_count) ? (slot + class_size) : nullptr;
        *(uint8_t **)slot = next;
        slot += class_size;
    }
    printf("slab: new page class=%zu phys=%p virt=%p free=%u\n",
           class_size, page_phys, page, hdr->free_count);
    return hdr;
}

void *SlabAllocator::alloc_from_page(slab_page *page) {
    if (!page || !validate_page(page, class_index(page->class_size))) return nullptr;
    if (!page->freelist) return nullptr;
    uint8_t *slot = page->freelist;
    page->freelist = *(uint8_t **)slot;
    page->free_count--;
    return slot;
}

void *SlabAllocator::allocate(size_t size, size_t align) {
    if (align > 16 && (align & (align - 1))) {
        /* wymagamy potęgi 2; fallback do minimalnej */
        align = 16;
    }
    int idx = class_index(size);
    if (idx < 0) return nullptr;
    size_t cls = class_size_for_idx(idx);
    slab_page *page = classes_[idx];
    slab_page *prev = nullptr;
    while (page) {
        if (!validate_page(page, idx)) {
            printf("slab: invalid page detected class=%zu at %p\n", cls, page);
        } else if (page->free_count) {
            break;
        }
        prev = page;
        page = page->next;
    }
    if (!page) {
        page = new_page(cls);
        if (!page) return nullptr;
        if (prev) prev->next = page;
        else classes_[idx] = page;
    }
    void *slot = alloc_from_page(page);
    if (!slot) {
        printf("slab: alloc_from_page failed class=%zu page=%p\n", cls, page);
        return nullptr;
    }
    return slot;
}

void SlabAllocator::deallocate(void *ptr) {
    slab_page *hdr = nullptr;
    int idx = -1;
    if (!validate_ptr(ptr, &hdr, &idx)) {
        printf("slab: invalid free ptr=%p\n", ptr);
        return;
    }
    *(uint8_t **)ptr = hdr->freelist;
    hdr->freelist = (uint8_t *)ptr;
    hdr->free_count++;
}


