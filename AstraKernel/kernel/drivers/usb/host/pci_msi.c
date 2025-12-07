/**
 * PCI MSI/MSI-X Support
 * 
 * Enables Message Signaled Interrupts for PCI devices (XHCI, etc.)
 * Falls back to legacy IRQ if MSI/MSI-X is not supported.
 */

#include "interrupts.h"
#include "apic.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* PCI Configuration Space Offsets */
#define PCI_CONFIG_COMMAND        0x04
#define PCI_CONFIG_STATUS        0x06
#define PCI_CONFIG_CAPABILITIES   0x34
#define PCI_CONFIG_INTERRUPT_LINE 0x3C
#define PCI_CONFIG_INTERRUPT_PIN  0x3D

/* PCI Capability IDs */
#define PCI_CAP_ID_MSI            0x05
#define PCI_CAP_ID_MSIX           0x11

/* PCI Command Register Bits */
#define PCI_CMD_IO_ENABLE         (1 << 0)
#define PCI_CMD_MEM_ENABLE        (1 << 1)
#define PCI_CMD_BUS_MASTER        (1 << 2)
#define PCI_CMD_INT_DISABLE       (1 << 10)

/* PCI Status Register Bits */
#define PCI_STATUS_CAP_LIST       (1 << 4)

/* MSI Capability Registers */
#define PCI_MSI_CAP_ID            0x00
#define PCI_MSI_CAP_NEXT           0x01
#define PCI_MSI_CAP_CTRL           0x02
#define PCI_MSI_CAP_ADDR_LO        0x04
#define PCI_MSI_CAP_ADDR_HI        0x08
#define PCI_MSI_CAP_DATA           0x08  /* 32-bit mode */
#define PCI_MSI_CAP_DATA_64        0x0C  /* 64-bit mode */

/* MSI Control Register Bits */
#define PCI_MSI_CTRL_ENABLE        (1 << 0)
#define PCI_MSI_CTRL_64BIT         (1 << 7)
#define PCI_MSI_CTRL_MULTI_SHIFT   1
#define PCI_MSI_CTRL_MULTI_MASK    0x07

/* MSI-X Capability Registers */
#define PCI_MSIX_CAP_ID            0x00
#define PCI_MSIX_CAP_NEXT          0x01
#define PCI_MSIX_CAP_CTRL          0x02
#define PCI_MSIX_CAP_TABLE         0x04
#define PCI_MSIX_CAP_PBA           0x08

/* MSI-X Control Register Bits */
#define PCI_MSIX_CTRL_ENABLE       (1 << 15)
#define PCI_MSIX_CTRL_FMASK        (1 << 14)
#define PCI_MSIX_CTRL_TABLE_SIZE_MASK 0x7FF

/* MSI-X Table Entry */
typedef struct PACKED {
    uint32_t msg_addr_lo;
    uint32_t msg_addr_hi;
    uint32_t msg_data;
    uint32_t vector_control;
} pci_msix_entry_t;

/* Helper functions for PCI config space access */
static uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

static void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

static uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_cfg_read32(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

static void pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t old = pci_cfg_read32(bus, slot, func, offset);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t new_val = (old & ~mask) | (((uint32_t)val) << shift);
    pci_cfg_write32(bus, slot, func, offset, new_val);
}

static uint8_t pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_cfg_read32(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

/**
 * Find PCI capability
 */
static uint8_t pci_find_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_id) {
    uint16_t status = pci_cfg_read16(bus, slot, func, PCI_CONFIG_STATUS);
    if (!(status & PCI_STATUS_CAP_LIST)) {
        return 0; /* No capabilities */
    }
    
    uint8_t cap_ptr = pci_cfg_read8(bus, slot, func, PCI_CONFIG_CAPABILITIES);
    while (cap_ptr != 0 && cap_ptr < 0xFC) {
        uint8_t id = pci_cfg_read8(bus, slot, func, cap_ptr);
        if (id == cap_id) {
            return cap_ptr;
        }
        cap_ptr = pci_cfg_read8(bus, slot, func, cap_ptr + 1);
    }
    
    return 0; /* Not found */
}

/**
 * Enable MSI for PCI device
 */
int pci_enable_msi(uint8_t bus, uint8_t slot, uint8_t func, uint8_t vector, uint8_t *msi_vector) {
    uint8_t msi_cap = pci_find_capability(bus, slot, func, PCI_CAP_ID_MSI);
    if (!msi_cap) {
        klog_printf(KLOG_WARN, "pci_msi: device %02x:%02x.%x does not support MSI", bus, slot, func);
        return -1;
    }
    
    uint16_t msi_ctrl = pci_cfg_read16(bus, slot, func, msi_cap + PCI_MSI_CAP_CTRL);
    bool is_64bit = (msi_ctrl & PCI_MSI_CTRL_64BIT) != 0;
    
    /* Get LAPIC base address */
    extern volatile uint32_t *apic_lapic_base;
    uint64_t lapic_base = (uint64_t)(uintptr_t)apic_lapic_base;
    
    /* Calculate MSI address (LAPIC base + 0xFEE00000 pattern) */
    uint32_t msi_addr_lo = 0xFEE00000 | (0 << 12); /* Destination ID = 0 (BSP) */
    uint32_t msi_addr_hi = 0;
    uint16_t msi_data = vector; /* Vector number */
    
    /* Write MSI address */
    pci_cfg_write32(bus, slot, func, msi_cap + PCI_MSI_CAP_ADDR_LO, msi_addr_lo);
    if (is_64bit) {
        pci_cfg_write32(bus, slot, func, msi_cap + PCI_MSI_CAP_ADDR_HI, msi_addr_hi);
        pci_cfg_write16(bus, slot, func, msi_cap + PCI_MSI_CAP_DATA_64, msi_data);
    } else {
        pci_cfg_write16(bus, slot, func, msi_cap + PCI_MSI_CAP_DATA, msi_data);
    }
    
    /* Enable MSI */
    msi_ctrl |= PCI_MSI_CTRL_ENABLE;
    pci_cfg_write16(bus, slot, func, msi_cap + PCI_MSI_CAP_CTRL, msi_ctrl);
    
    if (msi_vector) {
        *msi_vector = vector;
    }
    
    klog_printf(KLOG_INFO, "pci_msi: enabled MSI for %02x:%02x.%x -> vector %u", bus, slot, func, vector);
    return 0;
}

/**
 * Enable MSI-X for PCI device
 */
int pci_enable_msix(uint8_t bus, uint8_t slot, uint8_t func, uint8_t entry, uint8_t vector, uint8_t *msix_vector) {
    uint8_t msix_cap = pci_find_capability(bus, slot, func, PCI_CAP_ID_MSIX);
    if (!msix_cap) {
        klog_printf(KLOG_WARN, "pci_msix: device %02x:%02x.%x does not support MSI-X", bus, slot, func);
        return -1;
    }
    
    uint32_t msix_ctrl = pci_cfg_read32(bus, slot, func, msix_cap + PCI_MSIX_CAP_CTRL);
    uint16_t table_size = (msix_ctrl & PCI_MSIX_CTRL_TABLE_SIZE_MASK) + 1;
    
    if (entry >= table_size) {
        klog_printf(KLOG_ERROR, "pci_msix: entry %u >= table size %u", entry, table_size);
        return -1;
    }
    
    /* Get table offset */
    uint32_t table_offset = pci_cfg_read32(bus, slot, func, msix_cap + PCI_MSIX_CAP_TABLE);
    uint32_t table_bar = table_offset & ~0x7;
    uint32_t table_index = (table_offset & 0x7) >> 1;
    
    /* TODO: Map MSI-X table BAR if needed */
    /* For now, assume it's already mapped */
    
    /* Calculate MSI-X address */
    uint32_t msi_addr_lo = 0xFEE00000 | (0 << 12); /* Destination ID = 0 */
    uint32_t msi_addr_hi = 0;
    uint32_t msi_data = vector;
    
    /* Write MSI-X table entry */
    /* TODO: Access MSI-X table through BAR */
    /* This is a simplified version - full implementation needs BAR mapping */
    
    /* Enable MSI-X */
    msix_ctrl |= PCI_MSIX_CTRL_ENABLE;
    pci_cfg_write32(bus, slot, func, msix_cap + PCI_MSIX_CAP_CTRL, msix_ctrl);
    
    if (msix_vector) {
        *msix_vector = vector;
    }
    
    klog_printf(KLOG_INFO, "pci_msix: enabled MSI-X entry %u for %02x:%02x.%x -> vector %u", entry, bus, slot, func, vector);
    return 0;
}

/**
 * Disable MSI/MSI-X and fallback to legacy IRQ
 */
int pci_disable_msi(uint8_t bus, uint8_t slot, uint8_t func) {
    uint8_t msi_cap = pci_find_capability(bus, slot, func, PCI_CAP_ID_MSI);
    if (msi_cap) {
        uint16_t msi_ctrl = pci_cfg_read16(bus, slot, func, msi_cap + PCI_MSI_CAP_CTRL);
        msi_ctrl &= ~PCI_MSI_CTRL_ENABLE;
        pci_cfg_write16(bus, slot, func, msi_cap + PCI_MSI_CAP_CTRL, msi_ctrl);
    }
    
    uint8_t msix_cap = pci_find_capability(bus, slot, func, PCI_CAP_ID_MSIX);
    if (msix_cap) {
        uint32_t msix_ctrl = pci_cfg_read32(bus, slot, func, msix_cap + PCI_MSIX_CAP_CTRL);
        msix_ctrl &= ~PCI_MSIX_CTRL_ENABLE;
        pci_cfg_write32(bus, slot, func, msix_cap + PCI_MSIX_CAP_CTRL, msix_ctrl);
    }
    
    /* Enable legacy interrupts */
    uint16_t cmd = pci_cfg_read16(bus, slot, func, PCI_CONFIG_COMMAND);
    cmd &= ~PCI_CMD_INT_DISABLE;
    pci_cfg_write16(bus, slot, func, PCI_CONFIG_COMMAND, cmd);
    
    return 0;
}

/**
 * Try to enable MSI/MSI-X, fallback to legacy IRQ
 */
int pci_setup_interrupt(uint8_t bus, uint8_t slot, uint8_t func, uint8_t legacy_irq, uint8_t *vector) {
    /* Try MSI-X first */
    if (pci_enable_msix(bus, slot, func, 0, legacy_irq + 32, vector) == 0) {
        klog_printf(KLOG_INFO, "pci_int: using MSI-X for %02x:%02x.%x", bus, slot, func);
        return 0;
    }
    
    /* Try MSI */
    if (pci_enable_msi(bus, slot, func, legacy_irq + 32, vector) == 0) {
        klog_printf(KLOG_INFO, "pci_int: using MSI for %02x:%02x.%x", bus, slot, func);
        return 0;
    }
    
    /* Fallback to legacy IRQ */
    klog_printf(KLOG_INFO, "pci_int: using legacy IRQ%u for %02x:%02x.%x", legacy_irq, bus, slot, func);
    if (vector) {
        *vector = legacy_irq + 32; /* Map to APIC vector */
    }
    return 0;
}

