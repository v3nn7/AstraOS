/**
 * PCI USB Controller Detection
 * 
 * Scans PCI bus for USB controllers (UHCI, OHCI, EHCI, XHCI)
 * and registers them with USB core.
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/xhci.h>
#include "pci_msi.h"
#include "interrupts.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "vmm.h"
#include "memory.h"

/* External XHCI ops */
extern usb_host_ops_t xhci_ops;
extern uint64_t pmm_hhdm_offset;

/* PCI Configuration Space Access */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_cfg_read32(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

static uint8_t pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_cfg_read32(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

/* PCI Class Codes for USB Controllers */
#define PCI_CLASS_SERIAL           0x0C
#define PCI_SUBCLASS_USB           0x03
#define PCI_PROGIF_UHCI            0x00
#define PCI_PROGIF_OHCI            0x10
#define PCI_PROGIF_EHCI            0x20
#define PCI_PROGIF_XHCI            0x30

/* PCI Configuration Space Offsets */
#define PCI_CONFIG_VENDOR_ID       0x00
#define PCI_CONFIG_DEVICE_ID       0x02
#define PCI_CONFIG_CLASS_CODE      0x0B
#define PCI_CONFIG_SUBCLASS        0x0A
#define PCI_CONFIG_PROG_IF         0x09
#define PCI_CONFIG_BAR0            0x10
#define PCI_CONFIG_BAR1            0x14
#define PCI_CONFIG_INTERRUPT_LINE  0x3C

/**
 * Detect and register USB controllers from PCI
 */
int usb_pci_detect(void) {
    klog_printf(KLOG_INFO, "usb_pci: scanning PCI bus for USB controllers");
    
    uint32_t controllers_found = 0;
    uint32_t devices_scanned = 0;
    
    /* Scan all PCI devices */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor_id = pci_cfg_read16(bus, slot, func, PCI_CONFIG_VENDOR_ID);
                if (vendor_id == 0xFFFF) {
                    continue; /* No device */
                }
                
                devices_scanned++;
                uint16_t device_id = pci_cfg_read16(bus, slot, func, PCI_CONFIG_DEVICE_ID);
                uint8_t class_code = pci_cfg_read8(bus, slot, func, PCI_CONFIG_CLASS_CODE);
                uint8_t subclass = pci_cfg_read8(bus, slot, func, PCI_CONFIG_SUBCLASS);
                uint8_t prog_if = pci_cfg_read8(bus, slot, func, PCI_CONFIG_PROG_IF);
                
                klog_printf(KLOG_DEBUG, "usb_pci: device at %02x:%02x.%x VID:PID=%04x:%04x Class=%02x:%02x:%02x",
                           bus, slot, func, vendor_id, device_id, class_code, subclass, prog_if);
                
                /* Check if it's a USB controller */
                if (class_code == PCI_CLASS_SERIAL && subclass == PCI_SUBCLASS_USB) {
                    klog_printf(KLOG_INFO, "usb_pci: found USB controller at %02x:%02x.%x (prog_if=%02x)",
                               bus, slot, func, prog_if);
                    /* Only process XHCI controllers (others not implemented yet) */
                    if (prog_if != PCI_PROGIF_XHCI) {
                        /* Skip unsupported controllers - log once per type */
                        const char *type_name = "Unknown";
                        if (prog_if == PCI_PROGIF_UHCI) type_name = "UHCI";
                        else if (prog_if == PCI_PROGIF_OHCI) type_name = "OHCI";
                        else if (prog_if == PCI_PROGIF_EHCI) type_name = "EHCI";
                        
                        /* Only log first unsupported controller of each type to avoid spam */
                        static bool logged_uhci = false, logged_ohci = false, logged_ehci = false;
                        if (prog_if == PCI_PROGIF_UHCI && !logged_uhci) {
                            klog_printf(KLOG_INFO, "usb_pci: skipping unsupported %s controllers (only XHCI supported)", type_name);
                            logged_uhci = true;
                        } else if (prog_if == PCI_PROGIF_OHCI && !logged_ohci) {
                            klog_printf(KLOG_INFO, "usb_pci: skipping unsupported %s controllers (only XHCI supported)", type_name);
                            logged_ohci = true;
                        } else if (prog_if == PCI_PROGIF_EHCI && !logged_ehci) {
                            klog_printf(KLOG_INFO, "usb_pci: skipping unsupported %s controllers (only XHCI supported)", type_name);
                            logged_ehci = true;
                        }
                        continue;
                    }
                    
                    /* Limit to max 4 XHCI controllers */
                    if (controllers_found >= 4) {
                        klog_printf(KLOG_WARN, "usb_pci: max controllers reached, skipping");
                        break;
                    }
                    
                    uint32_t bar0 = pci_cfg_read32(bus, slot, func, PCI_CONFIG_BAR0);
                    uint32_t bar1 = pci_cfg_read32(bus, slot, func, PCI_CONFIG_BAR1);
                    uint8_t irq = pci_cfg_read8(bus, slot, func, PCI_CONFIG_INTERRUPT_LINE);
                    
                    usb_host_controller_t *hc = (usb_host_controller_t *)kmalloc(sizeof(usb_host_controller_t));
                    if (!hc) {
                        klog_printf(KLOG_ERROR, "usb_pci: failed to allocate controller");
                        continue;
                    }
                    
                    k_memset(hc, 0, sizeof(usb_host_controller_t));
                    hc->irq = irq;
                    
                    /* XHCI controller */
                    if (prog_if == PCI_PROGIF_XHCI) {
                        hc->type = USB_CONTROLLER_XHCI;
                        hc->name = "XHCI";
                        klog_printf(KLOG_INFO, "usb_pci: found XHCI controller at %02x:%02x.%x",
                                   bus, slot, func);
                        
                        /* Get MMIO base address */
                        if (bar0 & 0x01) {
                            /* I/O port - not supported for XHCI */
                            klog_printf(KLOG_WARN, "usb_pci: XHCI uses I/O ports (not supported)");
                            kfree(hc);
                            continue;
                        }
                        
                        /* Get physical address from BAR */
                        uint64_t phys_addr = (bar0 & ~0xF);
                        if (bar1 != 0) {
                            /* 64-bit address */
                            phys_addr = ((uint64_t)bar1 << 32) | (bar0 & ~0xF);
                        }
                        
                        if (phys_addr == 0) {
                            klog_printf(KLOG_ERROR, "usb_pci: XHCI BAR is zero - invalid MMIO address");
                            kfree(hc);
                            continue;
                        }
                        
                        klog_printf(KLOG_INFO, "usb_pci: XHCI BAR physical address 0x%016llx (BAR0=0x%08x BAR1=0x%08x)", 
                                   (unsigned long long)phys_addr, bar0, bar1);
                        
                        /* Map MMIO through HHDM or VMM */
                        /* XHCI MMIO is typically 64KB, map at least 4 pages (16KB) */
                        uint64_t mmio_size = 0x10000; /* 64KB */
                        uint64_t aligned_phys = phys_addr & ~(PAGE_SIZE - 1);
                        uint64_t offset = phys_addr & (PAGE_SIZE - 1);
                        
                        /* Try HHDM first (if BAR is in mapped range) */
                        if (phys_addr < 0x100000000ULL && pmm_hhdm_offset) {
                            /* Use HHDM if BAR is below 4GB and HHDM is available */
                            uint64_t virt_addr = pmm_hhdm_offset + phys_addr;
                            hc->regs_base = (void *)(uintptr_t)virt_addr;
                            klog_printf(KLOG_INFO, "usb_pci: XHCI using HHDM mapping: phys=0x%016llx virt=0x%016llx", 
                                       (unsigned long long)phys_addr, (unsigned long long)virt_addr);
                        } else {
                            /* Map via VMM */
                            uint64_t virt_base = 0xFFFF800000000000ULL + aligned_phys; /* Use high memory area */
                            klog_printf(KLOG_INFO, "usb_pci: mapping XHCI MMIO via VMM: phys=0x%016llx virt=0x%016llx size=0x%llx",
                                       (unsigned long long)aligned_phys, (unsigned long long)virt_base, (unsigned long long)mmio_size);
                            for (uint64_t i = 0; i < mmio_size; i += PAGE_SIZE) {
                                uint64_t phys_page = aligned_phys + i;
                                uint64_t virt_page = virt_base + i;
                                vmm_map(virt_page, phys_page, PAGE_WRITE | PAGE_CACHE_DISABLE);
                                klog_printf(KLOG_DEBUG, "usb_pci: mapped XHCI MMIO page phys=0x%016llx virt=0x%016llx",
                                           (unsigned long long)phys_page, (unsigned long long)virt_page);
                            }
                            hc->regs_base = (void *)(uintptr_t)(virt_base + offset);
                            klog_printf(KLOG_INFO, "usb_pci: XHCI mapped via VMM: phys=0x%016llx virt=0x%016llx", 
                                       (unsigned long long)phys_addr, (unsigned long long)(uintptr_t)hc->regs_base);
                        }
                        
                        if (!hc->regs_base) {
                            klog_printf(KLOG_ERROR, "usb_pci: failed to map XHCI MMIO");
                            kfree(hc);
                            continue;
                        }
                        
                        /* Setup interrupt (try MSI-X, then MSI, then legacy) */
                        uint8_t vector = 0;
                        if (pci_setup_interrupt(bus, slot, func, irq, &vector) == 0) {
                            hc->irq = vector;
                            klog_printf(KLOG_INFO, "usb_pci: XHCI interrupt setup -> vector %u", vector);
                        } else {
                            hc->irq = irq + 32; /* Fallback to legacy IRQ */
                            vector = hc->irq;
                            klog_printf(KLOG_WARN, "usb_pci: XHCI using legacy IRQ%u (vector %u)", irq, vector);
                        }
                        
                        /* Register XHCI interrupt handler */
                        extern void xhci_register_irq_handler(usb_host_controller_t *hc, uint8_t vector);
                        xhci_register_irq_handler(hc, vector);
                        
                        /* Set XHCI operations */
                        hc->ops = &xhci_ops;
                        
                        /* Register XHCI controller (this will also initialize it via ops->init) */
                        if (usb_host_register(hc) == 0) {
                            controllers_found++;
                            klog_printf(KLOG_INFO, "usb_pci: XHCI controller registered and initialized");
                        } else {
                            klog_printf(KLOG_ERROR, "usb_pci: failed to register XHCI controller");
                            pci_disable_msi(bus, slot, func);
                            kfree(hc);
                        }
                    }
                }
            }
        }
    }
    
    klog_printf(KLOG_INFO, "usb_pci: detection complete - scanned %u PCI devices, found %u USB controllers", 
                devices_scanned, controllers_found);
    return controllers_found > 0 ? 0 : -1;
}

