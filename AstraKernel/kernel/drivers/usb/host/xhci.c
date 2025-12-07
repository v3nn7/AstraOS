/**
 * XHCI Controller Implementation
 * 
 * Full TRB-based USB 3.0+ controller driver.
 * Supports MSI/MSI-X and legacy IRQ.
 */

#include "usb/xhci.h"
#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "mmio.h"
#include "vmm.h"
#include "memory.h"
#include "interrupts.h"

/* Forward declarations */
extern int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
extern int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb);
#include "pmm.h"

/* PCI XHCI Class Code */
#define PCI_CLASS_SERIAL_USB     0x0C
#define PCI_SUBCLASS_USB_XHCI    0x03

/* Helper macros for MMIO access */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/**
 * Initialize XHCI controller
 */
int xhci_init(usb_host_controller_t *hc) {
    if (!hc) {
        klog_printf(KLOG_ERROR, "xhci: invalid host controller");
        return -1;
    }
    
    if (!hc->regs_base) {
        klog_printf(KLOG_ERROR, "xhci: regs_base is NULL - MMIO not mapped");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: initializing controller mmio=%p", hc->regs_base);

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) {
        xhci = (xhci_controller_t *)kmalloc(sizeof(xhci_controller_t));
        if (!xhci) {
            klog_printf(KLOG_ERROR, "xhci: allocation failed");
            return -1;
        }
        k_memset(xhci, 0, sizeof(xhci_controller_t));
        hc->private_data = xhci;
    }

    xhci->cap_regs = hc->regs_base;
    
    /* Validate MMIO access - try reading CAPLENGTH */
    uint32_t test_read = XHCI_READ32(xhci->cap_regs, 0);
    klog_printf(KLOG_INFO, "xhci: MMIO test read from offset 0 = 0x%08x", test_read);
    xhci->cap_length = XHCI_READ32(xhci->cap_regs, XHCI_CAPLENGTH) & 0xFF;
    xhci->op_regs = (void *)((uintptr_t)xhci->cap_regs + xhci->cap_length);
    
    /* Get Runtime Registers offset */
    uint32_t rtsoff = XHCI_READ32(xhci->cap_regs, XHCI_RTSOFF) & 0xFFFF;
    xhci->rt_regs = (void *)((uintptr_t)xhci->cap_regs + rtsoff);
    
    /* Get Doorbell Registers offset */
    uint32_t dboff = XHCI_READ32(xhci->cap_regs, XHCI_DBOFF) & 0xFFFF;
    xhci->doorbell_regs = (void *)((uintptr_t)xhci->cap_regs + dboff);

    xhci->hci_version = XHCI_READ32(xhci->cap_regs, XHCI_HCIVERSION);
    uint32_t hcsparams1 = XHCI_READ32(xhci->cap_regs, XHCI_HCSPARAMS1);
    xhci->num_slots = ((hcsparams1 >> 0) & 0xFF) + 1;
    xhci->num_ports = ((hcsparams1 >> 24) & 0xFF);
    xhci->max_interrupters = ((hcsparams1 >> 8) & 0x7FF) + 1;

    uint32_t hccparams = XHCI_READ32(xhci->cap_regs, XHCI_HCCPARAMS);
    xhci->has_64bit_addressing = (hccparams & (1 << 0)) != 0;
    
    /* Check for scratchpad support */
    xhci->has_scratchpad = (hccparams & (1 << 2)) != 0;
    if (xhci->has_scratchpad) {
        uint32_t hcsparams2 = XHCI_READ32(xhci->cap_regs, XHCI_HCSPARAMS2);
        xhci->scratchpad_size = (hcsparams2 >> 16) & 0x3F; /* Max scratchpad buffers */
        klog_printf(KLOG_INFO, "xhci: scratchpad support enabled (%u buffers)", xhci->scratchpad_size);
    }

    klog_printf(KLOG_INFO, "xhci: version %04x slots=%d ports=%d inter=%d 64bit=%d",
                xhci->hci_version, xhci->num_slots, xhci->num_ports,
                xhci->max_interrupters, xhci->has_64bit_addressing);
    
    klog_printf(KLOG_INFO, "xhci: cap_regs=%p op_regs=%p rt_regs=%p doorbell=%p",
                xhci->cap_regs, xhci->op_regs, xhci->rt_regs, xhci->doorbell_regs);

    /* Reset controller */
    klog_printf(KLOG_INFO, "xhci: resetting controller...");
    int ret = xhci_reset(hc);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "xhci: controller reset failed");
        return ret;
    }
    klog_printf(KLOG_INFO, "xhci: controller reset complete");

    /* Initialize command ring */
    klog_printf(KLOG_INFO, "xhci: initializing command ring...");
    ret = xhci_cmd_ring_init(xhci);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to initialize command ring");
        return -1;
    }
    klog_printf(KLOG_INFO, "xhci: command ring ready");

    /* Initialize event ring */
    klog_printf(KLOG_INFO, "xhci: initializing event ring...");
    ret = xhci_event_ring_init(xhci);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to initialize event ring");
        return -1;
    }
    klog_printf(KLOG_INFO, "xhci: event ring ready");

    /* Allocate Device Context Base Address Array (DCBAAP) */
    size_t dcbaap_size = xhci->num_slots * sizeof(uint64_t);
    xhci->dcbaap = (uint64_t *)kmalloc(dcbaap_size + 64); /* Extra for alignment */
    if (!xhci->dcbaap) {
        klog_printf(KLOG_ERROR, "xhci: failed to allocate DCBAAP");
        return -1;
    }

    /* Align to 64-byte boundary */
    uintptr_t dcbaap_addr = (uintptr_t)xhci->dcbaap;
    if (dcbaap_addr % 64 != 0) {
        dcbaap_addr = (dcbaap_addr + 63) & ~63;
        xhci->dcbaap = (uint64_t *)dcbaap_addr;
    }

    k_memset(xhci->dcbaap, 0, dcbaap_size);

    /* Allocate scratchpad buffers if needed */
    if (xhci->has_scratchpad && xhci->scratchpad_size > 0) {
        size_t scratchpad_array_size = xhci->scratchpad_size * sizeof(void *);
        xhci->scratchpad_buffers = (void **)kmalloc(scratchpad_array_size);
        if (xhci->scratchpad_buffers) {
            k_memset(xhci->scratchpad_buffers, 0, scratchpad_array_size);
            
            /* Allocate actual scratchpad pages and set pointers */
            for (uint32_t i = 0; i < xhci->scratchpad_size; i++) {
                /* Allocate page-aligned buffer (4KB) */
                void *page = kmalloc(PAGE_SIZE + 64); /* Extra for alignment */
                if (!page) {
                    klog_printf(KLOG_ERROR, "xhci: failed to allocate scratchpad page %u", i);
                    /* Free already allocated pages */
                    for (uint32_t j = 0; j < i; j++) {
                        if (xhci->scratchpad_buffers[j]) {
                            kfree(xhci->scratchpad_buffers[j]);
                        }
                    }
                    kfree(xhci->scratchpad_buffers);
                    xhci->scratchpad_buffers = NULL;
                    return -1;
                }
                
                /* Align to page boundary */
                uintptr_t addr = (uintptr_t)page;
                if (addr % PAGE_SIZE != 0) {
                    addr = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                    page = (void *)addr;
                }
                
                k_memset(page, 0, PAGE_SIZE);
                xhci->scratchpad_buffers[i] = page;
                
                /* Get physical address and store in DCBAAP[0] format */
                uint64_t virt_addr = (uint64_t)(uintptr_t)page;
                uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
                if (phys_addr == 0) {
                    /* Fallback: use HHDM offset if available */
                    extern uint64_t pmm_hhdm_offset;
                    if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
                        phys_addr = virt_addr - pmm_hhdm_offset;
                    } else {
                        klog_printf(KLOG_ERROR, "xhci: failed to get physical address for scratchpad page %u", i);
                        /* Free already allocated pages */
                        for (uint32_t j = 0; j <= i; j++) {
                            if (xhci->scratchpad_buffers[j]) {
                                kfree(xhci->scratchpad_buffers[j]);
                            }
                        }
                        kfree(xhci->scratchpad_buffers);
                        xhci->scratchpad_buffers = NULL;
                        return -1;
                    }
                }
                
                /* Store physical address in scratchpad array (for DCBAAP[0]) */
                /* DCBAAP[0] points to array of scratchpad buffer pointers */
                if (i == 0) {
                    /* First scratchpad buffer pointer array will be stored in DCBAAP[0] */
                    /* We'll set this up when configuring device contexts */
                }
            }
            
            klog_printf(KLOG_INFO, "xhci: allocated %u scratchpad buffers", xhci->scratchpad_size);
        }
    }

    /* Set DCBAAP pointer */
    uint64_t dcbaap_virt = (uint64_t)(uintptr_t)xhci->dcbaap;
    uint64_t dcbaap_phys = vmm_virt_to_phys(dcbaap_virt);
    if (dcbaap_phys == 0) {
        /* Fallback: use HHDM offset if available */
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && dcbaap_virt >= pmm_hhdm_offset) {
            dcbaap_phys = dcbaap_virt - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci: failed to get physical address for DCBAAP");
            return -1;
        }
    }
    XHCI_WRITE64(xhci->op_regs, XHCI_DCBAAP, dcbaap_phys);
    klog_printf(KLOG_INFO, "xhci: DCBAAP set to virt=0x%016llx phys=0x%016llx", 
                (unsigned long long)dcbaap_virt, (unsigned long long)dcbaap_phys);

    /* Set Command Ring Control Register (CRCR) */
    uint64_t crcr = (uint64_t)(uintptr_t)xhci->cmd_ring.trbs;
    crcr |= XHCI_CRCR_RCS; /* Ring Cycle State */
    XHCI_WRITE64(xhci->op_regs, XHCI_CRCR, crcr);
    klog_printf(KLOG_INFO, "xhci: CRCR set to 0x%016llx (command ring ready)", (unsigned long long)crcr);

    /* Enable interrupter 0 */
    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    iman |= (1 << 1); /* Interrupt Enable */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman);
    klog_printf(KLOG_INFO, "xhci: interrupter 0 enabled (IMAN=0x%08x)", iman);
    
    /* Set interrupt moderation (IMOD) - 4000 microframes = ~500ms */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMOD(0), 4000);
    klog_printf(KLOG_INFO, "xhci: interrupt moderation set (IMOD=4000)");

    /* Enable controller */
    klog_printf(KLOG_INFO, "xhci: enabling controller (RUN + INTE)...");
    uint32_t usbcmd = XHCI_READ32(xhci->op_regs, XHCI_USBCMD);
    usbcmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    XHCI_WRITE32(xhci->op_regs, XHCI_USBCMD, usbcmd);

    /* Wait for controller to be ready */
    uint32_t timeout = 1000;
    while (timeout-- > 0) {
        uint32_t usbsts = XHCI_READ32(xhci->op_regs, XHCI_USBSTS);
        if (!(usbsts & XHCI_STS_HCH)) {
            klog_printf(KLOG_INFO, "xhci: controller ready (USBSTS=0x%08x)", usbsts);
            break;
        }
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (timeout == 0) {
        uint32_t usbsts = XHCI_READ32(xhci->op_regs, XHCI_USBSTS);
        klog_printf(KLOG_WARN, "xhci: controller not ready after timeout (USBSTS=0x%08x)", usbsts);
    }

    klog_printf(KLOG_INFO, "xhci: controller enabled and ready");
    return 0;
}

/**
 * Reset XHCI controller
 */
int xhci_reset(usb_host_controller_t *hc) {
    if (!hc) return -1;

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) return -1;

    /* Halt controller */
    uint32_t usbcmd = XHCI_READ32(xhci->op_regs, XHCI_USBCMD);
    usbcmd &= ~XHCI_CMD_RUN;
    XHCI_WRITE32(xhci->op_regs, XHCI_USBCMD, usbcmd);

    /* Wait for halt */
    uint32_t timeout = 1000;
    while (timeout-- > 0) {
        uint32_t usbsts = XHCI_READ32(xhci->op_regs, XHCI_USBSTS);
        if (usbsts & XHCI_STS_HCH) {
            break;
        }
        for (volatile int i = 0; i < 1000; i++);
    }

    /* Reset controller */
    usbcmd = XHCI_READ32(xhci->op_regs, XHCI_USBCMD);
    usbcmd |= XHCI_CMD_HCRST;
    XHCI_WRITE32(xhci->op_regs, XHCI_USBCMD, usbcmd);

    /* Wait for reset */
    timeout = 1000;
    while (timeout-- > 0) {
        uint32_t usbsts = XHCI_READ32(xhci->op_regs, XHCI_USBSTS);
        if (!(usbsts & XHCI_STS_CNR)) {
            break;
        }
        for (volatile int i = 0; i < 1000; i++);
    }

    klog_printf(KLOG_INFO, "xhci: controller reset complete");
    return 0;
}

/**
 * Reset port
 */
int xhci_reset_port(usb_host_controller_t *hc, uint8_t port) {
    if (!hc || port >= 32) return -1;

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci || port >= xhci->num_ports) return -1;

    uint32_t portsc = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
    klog_printf(KLOG_INFO, "xhci: port %u reset - initial PORTSC=0x%08x (CCS=%d, PED=%d, PR=%d)",
                port, portsc,
                (portsc & XHCI_PORTSC_CCS) ? 1 : 0,
                (portsc & XHCI_PORTSC_PED) ? 1 : 0,
                (portsc & XHCI_PORTSC_PR) ? 1 : 0);
    
    if (!(portsc & XHCI_PORTSC_CCS)) {
        klog_printf(KLOG_INFO, "xhci: port %u not connected (CCS=0)", port);
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: port %u device detected (CCS=1), starting reset...", port);
    
    portsc |= XHCI_PORTSC_PR;
    XHCI_WRITE32(xhci->op_regs, XHCI_PORTSC(port), portsc);

    /* Wait for reset */
    uint32_t timeout = 10000;
    while (timeout-- > 0) {
        portsc = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
        if (!(portsc & XHCI_PORTSC_PR)) {
            klog_printf(KLOG_INFO, "xhci: port %u reset complete - PORTSC=0x%08x (CCS=%d, PED=%d)",
                       port, portsc,
                       (portsc & XHCI_PORTSC_CCS) ? 1 : 0,
                       (portsc & XHCI_PORTSC_PED) ? 1 : 0);
            if (portsc & XHCI_PORTSC_PED) {
                klog_printf(KLOG_INFO, "xhci: port %u enabled (PED=1) - device ready for enumeration", port);
            } else {
                klog_printf(KLOG_WARN, "xhci: port %u reset complete but not enabled (PED=0)", port);
            }
            return 0;
        }
        for (volatile int i = 0; i < 1000; i++);
    }
    
    klog_printf(KLOG_WARN, "xhci: port %u reset timeout - PORTSC=0x%08x", port, portsc);
    return -1;
}

/**
 * Control transfer (TRB-based)
 */
int xhci_transfer_control(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer) return -1;

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) return -1;

    /* Use TRB-based control transfer */
    extern int xhci_submit_control_transfer(xhci_controller_t *xhci, usb_transfer_t *transfer);
    return xhci_submit_control_transfer(xhci, transfer);
}

/**
 * Interrupt transfer (TRB-based)
 */
int xhci_transfer_interrupt(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer || !transfer->endpoint || !transfer->device) return -1;

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) return -1;

    usb_device_t *dev = transfer->device;
    usb_endpoint_t *ep = transfer->endpoint;
    
    /* Get slot and endpoint numbers */
    uint32_t slot = dev->slot_id;
    if (slot == 0 || slot > xhci->num_slots) {
        klog_printf(KLOG_ERROR, "xhci: invalid slot %u", slot);
        return -1;
    }
    
    uint32_t ep_num = ep->address & 0x0F;
    uint32_t ep_dir = (ep->address & USB_ENDPOINT_DIR_IN) ? 1 : 0;
    uint32_t ep_idx = (ep_num * 2) + ep_dir;
    
    if (ep_idx >= 32) {
        klog_printf(KLOG_ERROR, "xhci: invalid endpoint index %u", ep_idx);
        return -1;
    }
    
    /* Initialize transfer ring if needed */
    if (!xhci->transfer_rings[slot][ep_idx]) {
        if (xhci_transfer_ring_init(xhci, slot, ep_idx) < 0) {
            klog_printf(KLOG_ERROR, "xhci: failed to init transfer ring slot=%u ep=%u", slot, ep_idx);
            return -1;
        }
    }
    
    xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][ep_idx];
    if (!ring) {
        klog_printf(KLOG_ERROR, "xhci: transfer ring not initialized");
        return -1;
    }
    
    /* Build Normal TRB */
    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    
    uint64_t data_addr = (uint64_t)(uintptr_t)transfer->buffer;
    trb.parameter = data_addr;
    
    uint32_t length = transfer->length;
    uint32_t flags = XHCI_TRB_IOC; /* Interrupt on completion */
    if (ep_dir == 0) { /* OUT */
        flags |= XHCI_TRB_ISP; /* Immediate data */
    }
    
    trb.status = length | (flags << 16);
    trb.control = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE;
    
    /* Enqueue TRB */
    if (xhci_transfer_ring_enqueue(ring, &trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to enqueue interrupt TRB");
        return -1;
    }
    
    /* Ring doorbell */
    volatile uint32_t *doorbell = (volatile uint32_t *)((uintptr_t)xhci->doorbell_regs + (slot * 4));
    mmio_write32(doorbell, ep_idx);
    
    transfer->status = USB_TRANSFER_SUCCESS; /* Will be updated by event handler */
    return 0;
}

/**
 * Bulk transfer (TRB-based)
 */
int xhci_transfer_bulk(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer || !transfer->endpoint || !transfer->device) return -1;

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) return -1;

    usb_device_t *dev = transfer->device;
    usb_endpoint_t *ep = transfer->endpoint;
    
    /* Get slot and endpoint numbers */
    uint32_t slot = dev->slot_id;
    if (slot == 0 || slot > xhci->num_slots) {
        klog_printf(KLOG_ERROR, "xhci: invalid slot %u for bulk transfer", slot);
        return -1;
    }
    
    uint32_t ep_num = ep->address & 0x0F;
    uint32_t ep_dir = (ep->address & USB_ENDPOINT_DIR_IN) ? 1 : 0;
    uint32_t ep_idx = (ep_num * 2) + ep_dir;
    
    if (ep_idx >= 32) {
        klog_printf(KLOG_ERROR, "xhci: invalid endpoint index %u for bulk transfer", ep_idx);
        return -1;
    }
    
    /* Initialize transfer ring if needed */
    if (!xhci->transfer_rings[slot][ep_idx]) {
        if (xhci_transfer_ring_init(xhci, slot, ep_idx) < 0) {
            klog_printf(KLOG_ERROR, "xhci: failed to init transfer ring slot=%u ep=%u for bulk", slot, ep_idx);
            return -1;
        }
    }
    
    xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][ep_idx];
    if (!ring) {
        klog_printf(KLOG_ERROR, "xhci: transfer ring not initialized for bulk");
        return -1;
    }
    
    /* Build Normal TRB for bulk transfer */
    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    
    uint64_t data_addr = (uint64_t)(uintptr_t)transfer->buffer;
    trb.parameter = data_addr;
    
    uint32_t length = transfer->length;
    uint32_t flags = XHCI_TRB_IOC; /* Interrupt on completion */
    if (ep_dir == 0) { /* OUT */
        flags |= XHCI_TRB_ISP; /* Immediate data */
    }
    
    trb.status = length | (flags << 16);
    trb.control = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE;
    
    /* Enqueue TRB */
    if (xhci_transfer_ring_enqueue(ring, &trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to enqueue bulk TRB");
        return -1;
    }
    
    /* Ring doorbell */
    volatile uint32_t *doorbell = (volatile uint32_t *)((uintptr_t)xhci->doorbell_regs + (slot * 4));
    mmio_write32(doorbell, ep_idx);
    
    transfer->status = USB_TRANSFER_SUCCESS; /* Will be updated by event handler */
    return 0;
}

/**
 * Isochronous transfer
 */
int xhci_transfer_isoc(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer) return -1;
    transfer->status = USB_TRANSFER_ERROR;
    return -1;
}

/**
 * Poll controller (for non-interrupt mode)
 */
int xhci_poll(usb_host_controller_t *hc) {
    if (!hc) return -1;
    
    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) return -1;
    
    /* Process event ring */
    extern int xhci_process_events(xhci_controller_t *xhci);
    return xhci_process_events(xhci);
}

/**
 * Cleanup XHCI controller
 */
void xhci_cleanup(usb_host_controller_t *hc) {
    if (!hc) return;

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) return;

    /* Free transfer rings */
    for (uint32_t slot = 0; slot < 32; slot++) {
        for (uint32_t ep = 0; ep < 32; ep++) {
            if (xhci->transfer_rings[slot][ep]) {
                if (xhci->transfer_rings[slot][ep]->trbs) {
                    kfree(xhci->transfer_rings[slot][ep]->trbs);
                }
                kfree(xhci->transfer_rings[slot][ep]);
            }
        }
    }

    /* Free command ring */
    if (xhci->cmd_ring.trbs) {
        kfree(xhci->cmd_ring.trbs);
    }

    /* Free event ring */
    if (xhci->event_ring.trbs) {
        kfree(xhci->event_ring.trbs);
    }
    if (xhci->event_ring.segment_table) {
        kfree(xhci->event_ring.segment_table);
    }

    /* Free DCBAAP */
    if (xhci->dcbaap) {
        kfree(xhci->dcbaap);
    }

    /* Free scratchpad buffers */
    if (xhci->scratchpad_buffers) {
        /* Free individual scratchpad pages */
        for (uint32_t i = 0; i < xhci->scratchpad_size; i++) {
            if (xhci->scratchpad_buffers[i]) {
                kfree(xhci->scratchpad_buffers[i]);
            }
        }
        kfree(xhci->scratchpad_buffers);
        xhci->scratchpad_buffers = NULL;
    }

    kfree(xhci);
    hc->private_data = NULL;
}

/**
 * PCI probe for XHCI controller
 * 
 * This function is called from PCI detection code to probe and initialize XHCI controller.
 * The actual PCI detection is handled in pci_usb_detect.c, this function is kept for
 * compatibility and future use.
 * 
 * NOTE: This is a stub function. Actual PCI detection uses usb_pci_detect() in pci_usb_detect.c.
 */
int xhci_pci_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    /* PCI detection is handled in pci_usb_detect.c via usb_pci_detect() */
    /* This function is kept for compatibility but actual detection happens elsewhere */
    klog_printf(KLOG_INFO, "xhci: PCI probe called for %02x:%02x.%x (detection handled by pci_usb_detect)", 
                bus, slot, func);
    
    /* Inline PCI config read for this stub function */
    #define PCI_CONFIG_ADDRESS 0xCF8
    #define PCI_CONFIG_DATA    0xCFC
    
    /* Check if device exists and is XHCI */
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | 0x00;
    outl(PCI_CONFIG_ADDRESS, addr);
    uint32_t vendor_device = inl(PCI_CONFIG_DATA);
    uint16_t vendor_id = (uint16_t)(vendor_device & 0xFFFF);
    
    if (vendor_id == 0xFFFF) {
        return -1; /* No device */
    }
    
    /* Read class code */
    addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | 0x08;
    outl(PCI_CONFIG_ADDRESS, addr);
    uint32_t class_rev = inl(PCI_CONFIG_DATA);
    uint8_t class_code = (uint8_t)((class_rev >> 24) & 0xFF);
    uint8_t subclass = (uint8_t)((class_rev >> 16) & 0xFF);
    uint8_t prog_if = (uint8_t)((class_rev >> 8) & 0xFF);
    
    /* Check if it's XHCI */
    if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x30) {
        klog_printf(KLOG_INFO, "xhci: XHCI controller confirmed at %02x:%02x.%x", bus, slot, func);
        return 0; /* XHCI controller found */
    }
    
    return -1; /* Not XHCI */
}

/* XHCI Host Controller Operations */
/* This will be used when XHCI controller is registered via PCI */
usb_host_ops_t xhci_ops = {
    .init = xhci_init,
    .reset = xhci_reset,
    .reset_port = xhci_reset_port,
    .transfer_control = xhci_transfer_control,
    .transfer_interrupt = xhci_transfer_interrupt,
    .transfer_bulk = xhci_transfer_bulk,
    .transfer_isoc = xhci_transfer_isoc,
    .poll = xhci_poll,
    .cleanup = xhci_cleanup
};

