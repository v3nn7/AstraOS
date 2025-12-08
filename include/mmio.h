#pragma once

#include <stdint.h>

static inline uint8_t inb(uint16_t port) { (void)port; return 0; }
static inline uint16_t inw(uint16_t port) { (void)port; return 0; }
static inline uint32_t inl(uint16_t port) { (void)port; return 0; }

static inline void outb(uint16_t port, uint8_t val) { (void)port; (void)val; }
static inline void outw(uint16_t port, uint16_t val) { (void)port; (void)val; }
static inline void outl(uint16_t port, uint32_t val) { (void)port; (void)val; }

static inline uint32_t mmio_read32(volatile uint32_t *addr) { return addr ? *addr : 0; }
static inline uint64_t mmio_read64(volatile uint64_t *addr) { return addr ? *addr : 0; }
static inline void mmio_write32(volatile uint32_t *addr, uint32_t val) { if (addr) *addr = val; }
static inline void mmio_write64(volatile uint64_t *addr, uint64_t val) { if (addr) *addr = val; }

