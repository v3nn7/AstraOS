#include <assert.h>
#include <kernel/kmain.h>
#include <kernel/panic.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/timer.h>
#include <drivers/pci.h>
#include <drivers/framebuffer.h>
#include <drivers/input.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <lib/utils.h>

int main(void) {
    gdt_init();
    idt_init();
    isr_init();
    cpu_init();
    pit_init();
    pci_init();
    framebuffer_init();
    input_init();
    keyboard_init();
    kmain();
    kernel_panic("test panic");

    assert(kstrlen("abc") == 3);
    assert(kstrlen("") == 0);
    kprintf("test %d", 1);
    delay_cycles(0);

    return 0;
}

