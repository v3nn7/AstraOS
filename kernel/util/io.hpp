// Basic port I/O helpers (no-ops under HOST_TEST).
#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
#ifndef HOST_TEST
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
#else
    (void)port;
    (void)value;
#endif
}

static inline uint8_t inb(uint16_t port) {
#ifndef HOST_TEST
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#else
    (void)port;
    return 0;
#endif
}

static inline void outl(uint16_t port, uint32_t value) {
#ifndef HOST_TEST
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
#else
    (void)port;
    (void)value;
#endif
}

static inline uint32_t inl(uint16_t port) {
#ifndef HOST_TEST
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#else
    (void)port;
    return 0;
#endif
}
