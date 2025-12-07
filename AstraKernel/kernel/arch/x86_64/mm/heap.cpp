#include "mm/metadata.hpp"
#include "mm/slab.hpp"
#include "mm/buddy.hpp"
#include "mm/dma_heap.hpp"
#include "heap_api.h"
#include "kernel.h"
#include "string.h"
#include "memory.h"
#include "pmm.h"
#include "vmm.h"

using namespace mm;

static SlabAllocator g_slab;
static BuddyAllocator g_buddy;
static DmaAllocator g_dma;

static bool g_initialized = false;

static void panic_corrupt(const char *msg, void *ptr) {
    printf("HEAP PANIC: %s ptr=%p\n", msg, ptr);
    while (1) { __asm__("cli; hlt"); }
}

extern "C" void heap_cpp_init(void) {
    if (g_initialized) {
        printf("heap: already initialized\n");
        return;
    }
    printf("heap: initializing slab allocator\n");
    g_slab.init();
    printf("heap: slab initialized\n");

    /* Przydziel 64MiB dla buddy z PMM (16384 stron) */
    printf("heap: allocating 64MB for buddy allocator (16384 pages)\n");
    void *buddy_phys = pmm_alloc_pages(16384);
    if (!buddy_phys) {
        printf("heap: ERROR - failed to allocate buddy region\n");
        panic_corrupt("buddy init: no memory", nullptr);
    }
    uint64_t buddy_base = (uint64_t)buddy_phys;
    size_t buddy_size = 64ULL * 1024 * 1024;
    printf("heap: buddy region allocated at phys=%p\n", (void *)buddy_base);

    /* CRITICAL: Map buddy region into HHDM via VMM to ensure it's accessible */
    printf("heap: mapping buddy region into HHDM\n");
    uint64_t buddy_virt = pmm_hhdm_offset + buddy_base;
    size_t pages_mapped = 0;
    for (uint64_t off = 0; off < buddy_size; off += PAGE_SIZE) {
        vmm_map(buddy_virt + off, buddy_base + off, PAGE_WRITE | PAGE_PRESENT);
        pages_mapped++;
        if (pages_mapped % 1000 == 0) {
            printf("heap: mapped %zu/%zu pages...\n", pages_mapped, buddy_size / PAGE_SIZE);
        }
    }
    printf("heap: buddy region mapped phys=%p virt=%p size=%zu (%zu pages)\n",
           (void *)buddy_base, (void *)buddy_virt, buddy_size, pages_mapped);

    printf("heap: initializing buddy allocator\n");
    g_buddy.init(buddy_base, buddy_size);
    printf("heap: buddy initialized\n");

    printf("heap: initializing DMA allocator\n");
    g_dma.init();
    printf("heap: DMA initialized\n");
    
    g_initialized = true;
    printf("heap: all allocators initialized\n");
}

static void *heap_alloc_internal(size_t size, size_t align, mm::block_tag tag) {
    if (!g_initialized) heap_cpp_init();
    if (align < MIN_ALIGN) align = MIN_ALIGN;
    if (align & (align - 1)) {
        /* must be power of two */
        size_t a = MIN_ALIGN;
        while (a < align) a <<= 1;
        align = a;
    }
    size_t total = size + sizeof(block_header);
    total = align_up(total, align);

    block_header *h = nullptr;
    void *payload = nullptr;

    switch (tag) {
        case mm::block_tag::SLAB:
            payload = g_slab.allocate(total, align);
            break;
        case mm::block_tag::DMA:
            payload = g_dma.allocate(total, align < 64 ? 64 : align);
            break;
        case mm::block_tag::BUDDY:
        case mm::block_tag::SAFE:
        default:
            payload = g_buddy.allocate(total, align);
            break;
    }
    if (!payload) return nullptr;
    
    /* Validate payload address based on tag */
    uint64_t payload_addr = (uint64_t)payload;
    if (tag == mm::block_tag::BUDDY || tag == mm::block_tag::SAFE) {
        /* Buddy returns HHDM addresses */
        if (payload_addr < pmm_hhdm_offset || payload_addr >= pmm_hhdm_offset + pmm_max_physical) {
            printf("heap: BUDDY returned invalid HHDM address %p\n", (void *)payload_addr);
            return nullptr;
        }
    } else if (tag == mm::block_tag::SLAB) {
        /* Slab returns HHDM addresses */
        if (payload_addr < pmm_hhdm_offset || payload_addr >= pmm_hhdm_offset + pmm_max_physical) {
            printf("heap: SLAB returned invalid HHDM address %p\n", (void *)payload_addr);
            return nullptr;
        }
    }
    
    h = reinterpret_cast<block_header *>(payload);
    h->size = (uint32_t)size;
    h->align = (uint16_t)align;
    h->tag = tag;
    fill_guards(h);
    printf("heap: alloc size=%zu align=%zu tag=%d payload=%p\n", size, align, (int)tag, payload);
    return header_to_ptr(h);
}

extern "C" void *heap_alloc(size_t size, size_t align, heap_block_tag_t ctag) {
    mm::block_tag tag = static_cast<mm::block_tag>(ctag);
    return heap_alloc_internal(size, align, tag);
}

extern "C" void heap_free(void *ptr) {
    if (!ptr) return;
    block_header *h = ptr_to_header(ptr);
    if (!h || !check_guards(h)) {
        panic_corrupt("heap_free: guard corrupted or invalid ptr", ptr);
    }
    size_t total = align_up(h->size + sizeof(block_header), h->align);
    switch (h->tag) {
        case mm::block_tag::SLAB:
            g_slab.deallocate(h);
            break;
        case mm::block_tag::DMA:
            g_dma.deallocate(h, total);
            break;
        case mm::block_tag::BUDDY:
        case mm::block_tag::SAFE:
        default:
            g_buddy.deallocate(h, total);
            break;
    }
}

extern "C" void *heap_realloc(void *ptr, size_t new_size) {
    /* realloc(NULL, size) is equivalent to malloc(size) */
    if (!ptr) {
        return heap_alloc(new_size, MIN_ALIGN, HEAP_TAG_SLAB);
    }
    
    /* realloc(ptr, 0) is equivalent to free(ptr) and return NULL */
    if (new_size == 0) {
        heap_free(ptr);
        return nullptr;
    }
    block_header *h = ptr_to_header(ptr);
    if (!h || !check_guards(h)) {
        panic_corrupt("heap_realloc: invalid pointer", ptr);
    }
    if (new_size <= h->size) return ptr;
    void *n = heap_alloc_internal(new_size, h->align, h->tag);
    if (!n) return nullptr;
    memcpy(n, ptr, h->size);
    heap_free(ptr);
    return n;
}

extern "C" void heap_stats(void) {
    printf("heap: stats not implemented yet\n");
}


