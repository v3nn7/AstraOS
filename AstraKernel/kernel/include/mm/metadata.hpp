#pragma once
/*
 * Memory block metadata shared across allocators.
 * Freestanding C++ (no exceptions/RTTI).
 */

#include "types.h"

namespace mm {

enum class block_tag : uint8_t {
    SLAB  = 0,
    BUDDY = 1,
    DMA   = 2,
    SAFE  = 3
};

struct alignas(16) block_header {
    uint64_t guard_front;
    uint32_t size;      // requested size
    uint16_t align;     // alignment used
    block_tag tag;
    uint64_t guard_back;
};

static constexpr uint64_t GUARD_VALUE = 0xDEADBEEFCAFEBABEULL;
static constexpr uint32_t MIN_ALIGN = 16;

inline size_t align_up(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

inline block_header *ptr_to_header(void *p) {
    if (!p) return nullptr;
    uint8_t *b = static_cast<uint8_t *>(p);
    block_header *h = reinterpret_cast<block_header *>(b - sizeof(block_header));
    return h;
}

inline void *header_to_ptr(block_header *h) {
    if (!h) return nullptr;
    return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(h) + sizeof(block_header));
}

inline void fill_guards(block_header *h) {
    h->guard_front = GUARD_VALUE;
    h->guard_back  = GUARD_VALUE;
}

inline bool check_guards(const block_header *h) {
    if (!h) return false;
    if (h->guard_front != GUARD_VALUE) return false;
    if (h->guard_back  != GUARD_VALUE) return false;
    return true;
}

} // namespace mm


