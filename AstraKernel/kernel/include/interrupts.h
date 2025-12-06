#pragma once

#include "types.h"

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

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

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
