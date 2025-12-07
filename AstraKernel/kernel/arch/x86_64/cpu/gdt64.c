#include "types.h"
#include "interrupts.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} PACKED gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} PACKED gdt_ptr_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} PACKED tss_t;

static struct PACKED {
    gdt_entry_t entries[5];
    uint64_t tss_low;
    uint64_t tss_high;
} gdt_table;

static gdt_ptr_t gdt_ptr;
tss_t tss; /* Make TSS visible to tss.c - must match tss_t in tss.c */

static void set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_entry_t *e = &gdt_table.entries[idx];
    e->limit_low = limit & 0xFFFF;
    e->base_low = base & 0xFFFF;
    e->base_mid = (base >> 16) & 0xFF;
    e->access = access;
    e->granularity = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    e->base_high = (base >> 24) & 0xFF;
}

static void set_tss_descriptor(void) {
    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof(tss) - 1;
    uint64_t low =
        (limit & 0xFFFF) |
        ((base & 0xFFFFFF) << 16) |
        ((uint64_t)0x89 << 40) |
        ((limit & 0xF0000ULL) << 32) |
        ((base & 0xFF000000ULL) << 32);
    uint64_t high = base >> 32;
    gdt_table.tss_low = low;
    gdt_table.tss_high = high;
}

void gdt_init(uint64_t stack_top) {
    /* Import TSS init function */
    extern void tss_init(uint64_t kernel_stack_top);
    
    for (int i = 0; i < 5; ++i) {
        gdt_table.entries[i] = (gdt_entry_t){0};
    }

    set_entry(0, 0, 0, 0, 0);
    set_entry(1, 0, 0, 0x9A, 0x20); /* kernel code: present, ring0, code, readable, long */
    set_entry(2, 0, 0, 0x92, 0x00); /* kernel data: present, ring0, data, writable */
    set_entry(3, 0, 0, 0xFA, 0x20); /* user code */
    set_entry(4, 0, 0, 0xF2, 0x00); /* user data */

    /* Initialize TSS properly */
    tss_init(stack_top);
    set_tss_descriptor();

    gdt_ptr.limit = sizeof(gdt_table) - 1;
    gdt_ptr.base = (uint64_t)&gdt_table;

    __asm__ volatile("lgdt %0" :: "m"(gdt_ptr));

    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushq $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        :
        : "rax", "memory");

    uint16_t tss_selector = 5 * 8;
    __asm__ volatile("ltr %0" :: "r"(tss_selector) : "memory");
}

