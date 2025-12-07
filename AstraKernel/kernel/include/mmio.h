#pragma once

#include "types.h"

static inline void memory_fence(void) {
    __asm__ volatile("mfence" ::: "memory");
}

static inline uint32_t mmio_read32(volatile uint32_t *addr) {
    uint32_t val = *addr;
    memory_fence();
    return val;
}

static inline void mmio_write32(volatile uint32_t *addr, uint32_t val) {
    memory_fence();
    *addr = val;
    memory_fence();
}

static inline uint64_t mmio_read64(volatile uint64_t *addr) {
    uint64_t val = *addr;
    memory_fence();
    return val;
}

static inline void mmio_write64(volatile uint64_t *addr, uint64_t val) {
    memory_fence();
    *addr = val;
    memory_fence();
}

