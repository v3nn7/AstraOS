/**
 * USB Initialization and Integration
 * 
 * Integrates USB stack with AstraOS kernel.
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_hid.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/xhci.h>
#include "drivers.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "mmio.h"
#include "kmalloc.h"

extern uint32_t pci_cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern int usb_pci_detect(void);
extern void usb_hid_scan_devices(void);

/* Helper macros for XHCI MMIO access */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((regs) + (offset)), val)

/**
 * Scan root hub ports for connected devices
 */
__attribute__((unused)) static int usb_scan_root_ports(usb_host_controller_t *hc) {
    if (!hc) return -1;

    klog_printf(KLOG_INFO, "usb: scanning root hub ports for controller %s", hc->name);

    if (hc->type == USB_CONTROLLER_XHCI) {
        xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
        if (!xhci) {
            klog_printf(KLOG_ERROR, "usb: xhci controller data missing");
            return -1;
        }
        if (!xhci->op_regs) {
            klog_printf(KLOG_ERROR, "usb: xhci op_regs is NULL, skipping scan");
            return -1;
        }

        klog_printf(KLOG_INFO, "usb: xhci op_regs=%lx num_ports=%u", (uintptr_t)xhci->op_regs, xhci->num_ports);
        klog_printf(KLOG_INFO, "usb: scanning %u XHCI ports...", xhci->num_ports);
        
        /* Scan all ports */
        for (uint8_t port = 0; port < xhci->num_ports; port++) {
            uint32_t portsc = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
            klog_printf(KLOG_INFO, "usb: port %u status: portsc=0x%08x (CCS=%d, PED=%d, PR=%d)", 
                       port, portsc,
                       (portsc & XHCI_PORTSC_CCS) ? 1 : 0,
                       (portsc & XHCI_PORTSC_PED) ? 1 : 0,
                       (portsc & XHCI_PORTSC_PR) ? 1 : 0);
            
            if (portsc & XHCI_PORTSC_CCS) { /* Port Connected */
                klog_printf(KLOG_INFO, "usb: port %u connected (portsc=0x%08x), resetting...", port, portsc);
                
                /* Reset port */
                klog_printf(KLOG_INFO, "usb: calling xhci_reset_port for port %u", port);
                if (xhci_reset_port(hc, port) == 0) {
                    klog_printf(KLOG_INFO, "usb: port %u reset initiated, waiting for enable...", port);
                    /* Wait for port to be enabled */
                    uint32_t timeout = 10000;
                    bool port_enabled = false;
                    while (timeout-- > 0) {
                        portsc = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
                        if (portsc & XHCI_PORTSC_PED) { /* Port Enabled */
                            port_enabled = true;
                            klog_printf(KLOG_INFO, "usb: port %u enabled after reset (portsc=0x%08x)", port, portsc);
                            break;
                        }
                        for (volatile int i = 0; i < 1000; i++);
                    }
                    
                    if (!port_enabled) {
                        portsc = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
                        klog_printf(KLOG_WARN, "usb: port %u did not enable after reset (portsc=0x%08x)", port, portsc);
                    }
                    
                    if (port_enabled) {
                        klog_printf(KLOG_INFO, "usb: port %u enabled, enumerating device...", port);
                        
                        /* Create device for enumeration */
                        extern usb_device_t *usb_device_alloc(void);
                        usb_device_t *dev = usb_device_alloc();
                        if (dev) {
                            dev->controller = hc;
                            dev->port = port;
                            dev->address = 0; /* Will be set during enumeration */
                            
                            /* Enumerate device (this will also call usb_bind_driver()) */
                            if (usb_device_enumerate(dev) == 0) {
                                klog_printf(KLOG_INFO, "usb: device enumerated successfully (address=%d)", dev->address);
                            } else {
                                klog_printf(KLOG_WARN, "usb: failed to enumerate device on port %u", port);
                                extern void usb_device_free(usb_device_t *dev);
                                usb_device_free(dev);
                            }
                        } else {
                            klog_printf(KLOG_WARN, "usb: failed to allocate device structure for port %u", port);
                        }
                    } else {
                        klog_printf(KLOG_WARN, "usb: port %u reset failed or port not enabled", port);
                    }
                } else {
                    klog_printf(KLOG_INFO, "usb: port %u not connected (CCS=0)", port);
                }
            }
        }
        
        klog_printf(KLOG_INFO, "usb: finished scanning XHCI ports");
    } else {
        klog_printf(KLOG_WARN, "usb: unsupported controller type %d for port scanning", hc->type);
    }
    
    return 0;
}

/**
 * Initialize USB subsystem
 */
void usb_init(void) {
    klog_printf(KLOG_INFO, "usb: initializing USB subsystem (enter)");

    /* Initialize USB core */
    if (usb_core_init() != 0) {
        klog_printf(KLOG_ERROR, "usb: failed to initialize core");
        return;
    }

    /* Initialize HID */
    if (usb_hid_init() != 0) {
        klog_printf(KLOG_ERROR, "usb: failed to initialize HID");
        return;
    }

    /* Register HID drivers */
    extern void usb_hid_register_drivers(void);
    usb_hid_register_drivers();
    
    /* Register hub driver */
    extern void usb_hub_register_driver(void);
    usb_hub_register_driver();

    /* Scan PCI for USB controllers */
    klog_printf(KLOG_INFO, "usb: starting PCI detection");
    if (usb_pci_detect() != 0) {
        klog_printf(KLOG_WARN, "usb: no USB controllers found");
        return;
    }
    klog_printf(KLOG_INFO, "usb: PCI detection finished");

    /* Scan root hub ports for connected devices */
    usb_host_controller_t *hc = usb_host_find_by_type(USB_CONTROLLER_XHCI);
    if (hc) {
        klog_printf(KLOG_WARN, "usb: skipping XHCI port scan (disabled to avoid MMIO hang)");
    } else {
        klog_printf(KLOG_WARN, "usb: no XHCI controller found, USB devices will not work");
    }

    klog_printf(KLOG_INFO, "usb: skipping HID device scan (USB disabled)");

    klog_printf(KLOG_INFO, "usb: initialization complete (exit)");
}

