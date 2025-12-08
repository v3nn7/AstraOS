#include <arch/x86_64/cpu.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>

void cpu_init(void) {
    gdt_init();
    idt_init();
}

