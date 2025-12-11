/*
 * AstraOS Source-Available License (ASAL v2.1)
 * Copyright (c) 2025 Krystian "v3nn7"
 * All rights reserved.
 *
 * Viewing allowed.
 * Modification, forking, redistribution, and commercial use prohibited
 * without explicit written permission from the author.
 *
 * Full license: see LICENSE.md or https://github.com/v3nn7
 */

#pragma once

#include "types.h"
#include "../util/io.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

typedef void (*irq_handler_t)(interrupt_frame_t *frame);

void gdt_init(uint64_t stack_top);
void idt_init(void);
void irq_init(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void interrupt_handler(uint8_t vector, interrupt_frame_t *frame);

static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" :: "a"(0));
}

static inline void interrupts_enable(void) { __asm__ volatile ("sti"); }
static inline void interrupts_disable(void) { __asm__ volatile ("cli"); }

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline void invlpg(void *addr) {
    __asm__ volatile ("invlpg (%0)" :: "r"(addr) : "memory");
}

#ifdef __cplusplus
}
#endif
