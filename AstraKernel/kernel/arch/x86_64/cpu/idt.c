#include "types.h"
#include "interrupts.h"
#include "kernel.h"

typedef struct PACKED {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} idt_entry_t;

typedef struct PACKED {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idtr;

static void set_gate(uint8_t vec, void *handler, uint8_t flags) {
    uint64_t addr = (uint64_t)handler;

    idt[vec].offset_low  = addr & 0xFFFF;
    idt[vec].selector    = 0x08;
    idt[vec].ist         = 0;
    idt[vec].type_attr   = flags;
    idt[vec].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero        = 0;
}

extern void isr0(interrupt_frame_t *);  extern void isr1(interrupt_frame_t *);
extern void isr2(interrupt_frame_t *);  extern void isr3(interrupt_frame_t *);
extern void isr4(interrupt_frame_t *);  extern void isr5(interrupt_frame_t *);
extern void isr6(interrupt_frame_t *);  extern void isr7(interrupt_frame_t *);
extern void isr8(interrupt_frame_t *, uint64_t);  extern void isr9(interrupt_frame_t *);
extern void isr10(interrupt_frame_t *, uint64_t); extern void isr11(interrupt_frame_t *, uint64_t);
extern void isr12(interrupt_frame_t *, uint64_t); extern void isr13(interrupt_frame_t *, uint64_t);
extern void isr14(interrupt_frame_t *, uint64_t); extern void isr15(interrupt_frame_t *);
extern void isr16(interrupt_frame_t *); extern void isr17(interrupt_frame_t *, uint64_t);
extern void isr18(interrupt_frame_t *); extern void isr19(interrupt_frame_t *);
extern void isr20(interrupt_frame_t *); extern void isr30(interrupt_frame_t *, uint64_t);

extern void irq32(interrupt_frame_t *);
extern void irq33(interrupt_frame_t *);
extern void irq34(interrupt_frame_t *);
extern void irq35(interrupt_frame_t *);
extern void irq36(interrupt_frame_t *);
extern void irq37(interrupt_frame_t *);
extern void irq38(interrupt_frame_t *);
extern void irq39(interrupt_frame_t *);
extern void irq40(interrupt_frame_t *);
extern void irq41(interrupt_frame_t *);
extern void irq42(interrupt_frame_t *);
extern void irq43(interrupt_frame_t *);
extern void irq44(interrupt_frame_t *);
extern void irq45(interrupt_frame_t *);
extern void irq46(interrupt_frame_t *);
extern void irq47(interrupt_frame_t *);

__attribute__((interrupt))
static void default_handler(interrupt_frame_t *f) {
    printf("Unhandled interrupt RIP=%p\n", (void *)f->rip);
    while (1) __asm__("cli; hlt");
}

void idt_init(void) {
    uint8_t trap = 0x8F;
    uint8_t intr = 0x8E;

    for (int i = 0; i < 256; i++)
        set_gate(i, default_handler, intr);

    // CPU exceptions
    set_gate(0, isr0, trap);   set_gate(1, isr1, trap);
    set_gate(2, isr2, trap);   set_gate(3, isr3, trap);
    set_gate(4, isr4, trap);   set_gate(5, isr5, trap);
    set_gate(6, isr6, trap);   set_gate(7, isr7, trap);
    set_gate(8, isr8, trap);   set_gate(9, isr9, trap);
    set_gate(10, isr10, trap); set_gate(11, isr11, trap);
    set_gate(12, isr12, trap); set_gate(13, isr13, trap);
    set_gate(14, isr14, trap); set_gate(15, isr15, trap);
    set_gate(16, isr16, trap); set_gate(17, isr17, trap);
    set_gate(18, isr18, trap); set_gate(19, isr19, trap);
    set_gate(20, isr20, trap); set_gate(30, isr30, trap);

    // IRQ (APIC)
    set_gate(32, irq32, intr);
    set_gate(33, irq33, intr);
    set_gate(34, irq34, intr);
    set_gate(35, irq35, intr);
    set_gate(36, irq36, intr);
    set_gate(37, irq37, intr);
    set_gate(38, irq38, intr);
    set_gate(39, irq39, intr);
    set_gate(40, irq40, intr);
    set_gate(41, irq41, intr);
    set_gate(42, irq42, intr);
    set_gate(43, irq43, intr);
    set_gate(44, irq44, intr);
    set_gate(45, irq45, intr);
    set_gate(46, irq46, intr);
    set_gate(47, irq47, intr);

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt[0];

    __asm__ volatile("lidt %0" :: "m"(idtr));
    __asm__ volatile("sti");
}
