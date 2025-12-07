#include "apic.h"
#include "mmio.h"
#include "memory.h"
#include "klog.h"
#include "kernel.h"

#define IOAPIC_DEFAULT_PHYS 0xFEC00000

extern volatile uint32_t *apic_ioapic_base;
extern volatile uint32_t *apic_lapic_base;

static inline volatile uint32_t *ioapic_sel(void) {
    return (volatile uint32_t *)((uintptr_t)apic_ioapic_base + 0x00);
}
static inline volatile uint32_t *ioapic_win(void) {
    return (volatile uint32_t *)((uintptr_t)apic_ioapic_base + 0x10);
}

static uint32_t ioapic_read(uint8_t reg) {
    mmio_write32(ioapic_sel(), reg);
    return mmio_read32(ioapic_win());
}

static void ioapic_write(uint8_t reg, uint32_t val) {
    mmio_write32(ioapic_sel(), reg);
    mmio_write32(ioapic_win(), val);
}

void ioapic_init(void) {
    if (!apic_ioapic_base) apic_mock_set_bases(apic_lapic_base, (volatile uint32_t *)(pmm_hhdm_offset + IOAPIC_DEFAULT_PHYS));
    uint32_t ver = ioapic_read(1);
    uint32_t max_redir = (ver >> 16) & 0xFF;
    klog_printf(KLOG_INFO, "ioapic: version=%x entries=%u base=%p", ver & 0xFF, max_redir + 1, apic_ioapic_base);
}

void ioapic_redirect_irq(uint8_t irq, uint8_t vector) {
    /* DIAGNOSTIC: Print before redirect */
    printf("IOAPIC: routing IRQ%u to vector %u\n", irq, vector);
    
    uint8_t reg = 0x10 + irq * 2;
    
    /* Low 32 bits: vector + delivery mode */
    uint32_t low = vector | (1 << 13); /* Level-triggered, active low */
    
    /* High 32 bits: destination LAPIC ID (0 = BSP) */
    uint32_t high = 0;
    
    printf("IOAPIC: writing reg=0x%02x low=0x%08x high=0x%08x\n", reg, low, high);
    ioapic_write(reg, low);
    ioapic_write(reg + 1, high);
    
    /* Verify write */
    uint32_t verify_low = ioapic_read(reg);
    uint32_t verify_high = ioapic_read(reg + 1);
    printf("IOAPIC: verified reg=0x%02x low=0x%08x high=0x%08x\n", reg, verify_low, verify_high);
    
    klog_printf(KLOG_INFO, "ioapic: redirected IRQ%u -> vector %u (reg=0x%02x)", irq, vector, reg);
}
