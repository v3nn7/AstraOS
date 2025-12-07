/*
 * AstraOS - TSS (Task State Segment) Management
 * 
 * In x86_64, TSS is required for:
 * - Stack switching during interrupts (RSP0, RSP1, RSP2, IST)
 * - I/O permission bitmap (optional)
 * 
 * TSS must be properly initialized and RSP0 must be updated
 * when kernel stack changes, otherwise interrupts will fail.
 */

#include "types.h"
#include "kernel.h"
#include "kmalloc.h"

/* Forward declaration - TSS structure is defined in gdt64.c */
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

/* External TSS instance from GDT */
extern tss_t tss;

/* Current kernel stack pointer */
static uint64_t current_kernel_rsp = 0;

/**
 * Update RSP0 in TSS (kernel stack pointer)
 * This MUST be called whenever kernel stack changes
 * @param rsp: New kernel stack pointer
 */
void tss_set_rsp0(uint64_t rsp) {
    tss.rsp0 = rsp;
    current_kernel_rsp = rsp;
    printf("tss: updated RSP0 to %p\n", (void *)rsp);
}

/**
 * Get current RSP0
 */
uint64_t tss_get_rsp0(void) {
    return tss.rsp0;
}

/**
 * Set IST entry (Interrupt Stack Table)
 * IST entries are used for specific interrupts that need
 * their own stack (e.g., double fault, NMI)
 * @param index: IST index (1-7)
 * @param rsp: Stack pointer for this IST entry
 */
void tss_set_ist(uint8_t index, uint64_t rsp) {
    if (index < 1 || index > 7) {
        printf("tss: ERROR - invalid IST index %u (must be 1-7)\n", index);
        return;
    }
    tss.ist[index - 1] = rsp;
    printf("tss: set IST[%u] = %p\n", index, (void *)rsp);
}

/**
 * Initialize TSS with kernel stack
 * This should be called during early boot
 * @param kernel_stack_top: Top of kernel stack
 */
void tss_init(uint64_t kernel_stack_top) {
    /* Zero out TSS using memset-like approach */
    uint64_t *ptr = (uint64_t *)&tss;
    for (size_t i = 0; i < sizeof(tss_t) / sizeof(uint64_t); i++) {
        ptr[i] = 0;
    }
    
    /* Set kernel stack pointer */
    tss.rsp0 = kernel_stack_top;
    current_kernel_rsp = kernel_stack_top;
    
    /* Set I/O map base to end of TSS (no I/O bitmap) */
    tss.io_map_base = sizeof(tss_t);
    
    /* Clear IST entries (not used for now) */
    for (int i = 0; i < 7; i++) {
        tss.ist[i] = 0;
    }
    
    printf("tss: initialized RSP0=%p io_map_base=%u\n", 
           (void *)kernel_stack_top, tss.io_map_base);
}

/**
 * Update TSS RSP0 from current RSP
 * Call this after stack switches
 */
void tss_update_from_current_rsp(void) {
    uint64_t rsp;
    __asm__ volatile("movq %%rsp, %0" : "=r"(rsp));
    tss_set_rsp0(rsp);
}
