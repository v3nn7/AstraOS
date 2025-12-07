/*
 * xHCI Host Controller Driver - Clean Implementation
 *
 * - USB 3.0+ (xHCI) DRVH
 * - TRB and Ring Management, Reset, Port Control, Transfer Routines
 * - Robust BIOS Handoff, Cache Flush (DMA-safe), Comprehensive Error Handling
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
#include "pmm.h"

/* xHCI PCI Class Codes */
#define PCI_CLASS_SERIAL_USB   0x0C
#define PCI_SUBCLASS_USB_XHCI  0x03

/* MMIO Access Helpers */
#define XHCI_READ32(r, off)     mmio_read32((volatile uint32_t *)((uintptr_t)(r) + (off)))
#define XHCI_WRITE32(r, off, v) mmio_write32((volatile uint32_t *)((uintptr_t)(r) + (off)), v)
#define XHCI_READ64(r, off)     mmio_read64((volatile uint64_t *)((uintptr_t)(r) + (off)))
#define XHCI_WRITE64(r, off, v) mmio_write64((volatile uint64_t *)((uintptr_t)(r) + (off)), v)

/* DMA-safe flush for TRB regions (DCBAAP, Rings, etc) */
static void xhci_flush_cache(void *addr, size_t sz) {
    uintptr_t s = (uintptr_t)addr;
    uintptr_t e = s + sz;
    s &= ~((uintptr_t)63);
    while (s < e) {
        __asm__ volatile("clflush (%0)" :: "r"(s) : "memory");
        s += 64;
    }
    __asm__ volatile("mfence" ::: "memory");
}

/* Perform BIOS/OS Handoff - disables SMI, ensures full OS ownership */
static int xhci_perform_bios_handoff(xhci_controller_t *xhci) {
    if (!xhci || !xhci->cap_regs) return -1;
    uint32_t hccp1 = xhci->cap_regs->HCCPARAMS1;
    uint32_t xecp = ((hccp1 >> 16) & 0xFFFF) << 2;
    if (!xecp) return 0;

    uintptr_t c = (uintptr_t)xhci->cap_regs + xecp;
    while (1) {
        uint32_t v = mmio_read32((volatile uint32_t*)c);
        uint8_t id = v & 0xFF;
        if (id == 1) {
            klog_printf(KLOG_INFO, "xhci: USB Legacy Support (ECID=1) at 0x%x", xecp);
            if (v & (1 << 16)) {
                klog_printf(KLOG_INFO, "xhci: Requesting OS ownership...");
                mmio_write32((volatile uint32_t*)c, v | (1<<24));
                int t = 10000;
                while (t-->0) {
                    v = mmio_read32((volatile uint32_t*)c);
                    if (!(v & (1<<16))) break;
                    for (volatile int i=0;i<1000;i++);
                }
                if (t <= 0) {
                    klog_printf(KLOG_WARN,"xhci: BIOS handoff timed out");
                    uint32_t ctl = mmio_read32((volatile uint32_t*)(c+4));
                    ctl &= ~0xE000;
                    mmio_write32((volatile uint32_t*)(c+4), ctl);
                } else {
                    klog_printf(KLOG_INFO, "xhci: BIOS handoff OK");
                }
            }
        }
        uint32_t next = (v>>8) & 0xFF;
        if (!next) break;
        c += (next<<2);
    }
    return 0;
}

/* xHCI Initialization - alloc/rings, parse caps, setup controller */
int xhci_init(usb_host_controller_t *hc) {
    if (!hc)            { klog_printf(KLOG_ERROR, "xhci: null input"); return -1; }
    if (!hc->regs_base)  { klog_printf(KLOG_ERROR, "xhci: regs_base unset"); return -1; }
    klog_printf(KLOG_INFO, "xhci: begin mmio=%p", hc->regs_base);

    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) {
        xhci = (xhci_controller_t*)kmalloc(sizeof(xhci_controller_t));
        if (!xhci) { klog_printf(KLOG_ERROR,"xhci: alloc fail"); return -1; }
        k_memset(xhci, 0, sizeof(xhci_controller_t));
        hc->private_data = xhci;
    }

    void *mmio_base = hc->regs_base;
    xhci_cap_regs_t *cap = (xhci_cap_regs_t*)mmio_base;
    xhci->cap_regs = cap;
    
    uint32_t caplen = cap->CAPLENGTH & 0xFF;
    xhci->cap_length = caplen;
    xhci->op_regs = (xhci_op_regs_t *)((uintptr_t)mmio_base + caplen);
    
    uint32_t rtsoff = cap->RTSOFF & ~0x1F;   // remove lowest 5 bits
    xhci->rt_regs = (xhci_rt_regs_t *)((uintptr_t)mmio_base + rtsoff);
    
    uint32_t dboff = cap->DBOFF & ~0x3;   // remove lowest 2 bits
    xhci->doorbell_regs = (xhci_doorbell_regs_t *)((uintptr_t)mmio_base + dboff);

    xhci->hci_version = xhci->cap_regs->HCIVERSION;
    uint32_t p1 = xhci->cap_regs->HCSPARAMS1;
    xhci->num_slots = p1 & 0xFF;
    xhci->num_ports = (p1 >> 24) & 0xFF;
    xhci->max_interrupters = (p1 >> 8) & 0x7FF;

    uint32_t hccp = xhci->cap_regs->HCCPARAMS1;
    xhci->has_64bit_addressing = (hccp & 1)?1:0;

    xhci->has_scratchpad = (hccp & (1<<2)) ? 1 : 0;
    if (xhci->has_scratchpad) {
        uint32_t p2 = xhci->cap_regs->HCSPARAMS2;
        xhci->scratchpad_size = ((p2 >> 21) & 0x1F)<<5 | ((p2>>16)&0x1F);
        klog_printf(KLOG_INFO,"xhci: scratchpad enabled (%u buf)",xhci->scratchpad_size);
    }

    klog_printf(KLOG_INFO, "xhci: v%04x slots=%d ports=%d ints=%d 64b=%d",
        xhci->hci_version, xhci->num_slots, xhci->num_ports, xhci->max_interrupters, xhci->has_64bit_addressing);

    if (xhci_perform_bios_handoff(xhci) < 0) {
        klog_printf(KLOG_ERROR,"xhci: BIOS handoff error");
        return -1;
    }

    klog_printf(KLOG_INFO,"xhci: reset ctrl...");
    int ret = xhci_reset(hc); if (ret < 0) { klog_printf(KLOG_ERROR,"xhci: reset fail"); return ret; }

    xhci->op_regs->CONFIG = xhci->num_slots;
    klog_printf(KLOG_INFO,"xhci: max slots set=%d",xhci->num_slots);

    klog_printf(KLOG_INFO,"xhci: cmd ring...");
    ret = xhci_cmd_ring_init(xhci);
    if (ret < 0) { klog_printf(KLOG_ERROR, "xhci: command ring fail"); return -1; }

    klog_printf(KLOG_INFO,"xhci: event ring...");
    ret = xhci_event_ring_init(xhci);
    if (ret < 0) { klog_printf(KLOG_ERROR, "xhci: event ring fail"); return -1; }

    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    iman |= (1<<1); iman &= ~(1<<0);
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman);
    __asm__ volatile("mfence" ::: "memory");
    klog_printf(KLOG_INFO,"xhci: IMAN0=0x%08x", XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0)));

    // DCBAAP array (slots+1) x 8 bytes (aligned 64)
    size_t dc_sz = (xhci->num_slots + 1) * sizeof(uint64_t);
    void *dc_alloc = kmalloc(dc_sz + 64);
    if (!dc_alloc) { klog_printf(KLOG_ERROR, "xhci: DCBAAP alloc fail"); return -1; }
    uintptr_t dc_addr = (uintptr_t)dc_alloc;
    if (dc_addr & 63) dc_addr = (dc_addr + 63) & ~63UL;
    xhci->dcbaap = (uint64_t*)dc_addr;
    k_memset(xhci->dcbaap, 0, dc_sz);
    xhci_flush_cache(xhci->dcbaap, dc_sz);

    // Scratchpad? Allocate pointer array, pages, setup DCBAAP[0]
    if (xhci->has_scratchpad && xhci->scratchpad_size) {
        xhci->scratchpad_buffers = (void**)kmalloc(xhci->scratchpad_size * sizeof(void*));
        if (!xhci->scratchpad_buffers) { klog_printf(KLOG_ERROR,"xhci: scratchpad bufptr fail"); return -1; }
        void *scratch_arr = kmalloc(PAGE_SIZE);
        if (!scratch_arr) { klog_printf(KLOG_ERROR,"xhci: scratchpage alloc fail"); return -1;}
        k_memset(scratch_arr, 0, PAGE_SIZE);
        uint64_t *sptr = (uint64_t*)scratch_arr;
        for (uint32_t i=0;i<xhci->scratchpad_size;i++) {
            void *p = kmalloc(PAGE_SIZE);
            if (!p) { klog_printf(KLOG_ERROR, "xhci: scratch %u alloc", i); return -1;}
            k_memset(p, 0, PAGE_SIZE);
            xhci_flush_cache(p, PAGE_SIZE);
            xhci->scratchpad_buffers[i] = p;
            sptr[i] = vmm_virt_to_phys((uintptr_t)p);
        }
        xhci_flush_cache(sptr,PAGE_SIZE);
        uint64_t phys_base = vmm_virt_to_phys((uintptr_t)sptr);
        xhci->dcbaap[0] = phys_base;
        xhci_flush_cache(&xhci->dcbaap[0],sizeof(uint64_t));
        klog_printf(KLOG_INFO,"xhci: scratchpad %u pages ok", xhci->scratchpad_size);
    }

    uint64_t dcb_virt = (uint64_t)(uintptr_t)xhci->dcbaap;
    uint64_t dcb_phys = vmm_virt_to_phys(dcb_virt);
    xhci->op_regs->DCBAAP = dcb_phys;
    __asm__ volatile("mfence" ::: "memory");
    klog_printf(KLOG_INFO,"xhci: DCBAAP virt=0x%016llx phys=0x%016llx",
            (unsigned long long)dcb_virt, (unsigned long long)dcb_phys);

    XHCI_WRITE32(xhci->rt_regs, XHCI_IMOD(0), 4000);

    klog_printf(KLOG_INFO,"xhci: enabling ctrl (RUN|INTE)...");
    uint32_t usbcmd = xhci->op_regs->USBCMD;
    usbcmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    xhci->op_regs->USBCMD = usbcmd;
    __asm__ volatile("mfence" ::: "memory");

    uint32_t tout = 1000;
    while (tout-- > 0) {
        uint32_t sts = xhci->op_regs->USBSTS;
        if (!(sts & XHCI_STS_HCH)) {
            klog_printf(KLOG_INFO,"xhci: controller running (USBSTS=0x%08x)", sts); break;
        }
        for (volatile int i=0;i<1000;i++);
    }
    if (tout == 0) {
        uint32_t sts = xhci->op_regs->USBSTS;
        klog_printf(KLOG_WARN,"xhci: ctrl not running (USBSTS=0x%08x)",sts);
        if (sts & (1<<12)) klog_printf(KLOG_ERROR,"xhci: Ctrl Error Detected!");
        if (sts & (1<<2))  klog_printf(KLOG_ERROR,"xhci: Host Sys Err!");
        return -1;
    }

    klog_printf(KLOG_INFO,"xhci: init OK");
    return 0;
}

int xhci_reset(usb_host_controller_t *hc) {
    if (!hc) return -1;
    xhci_controller_t *xhci = (xhci_controller_t*)hc->private_data;
    uint32_t cmd = xhci->op_regs->USBCMD;
    cmd &= ~XHCI_CMD_RUN;
    xhci->op_regs->USBCMD = cmd;
    __asm__ volatile("mfence" ::: "memory");

    uint32_t t = 2000;
    while (t-- > 0) {
        if (xhci->op_regs->USBSTS & XHCI_STS_HCH) break;
        for (volatile int i=0;i<1000;i++);
    }
    if (!t) klog_printf(KLOG_ERROR,"xhci: halt fail");

    cmd = xhci->op_regs->USBCMD;
    cmd |= XHCI_CMD_HCRST;
    xhci->op_regs->USBCMD = cmd;
    __asm__ volatile("mfence" ::: "memory");

    t = 2000;
    while (t-- > 0) {
        if (!(xhci->op_regs->USBCMD & XHCI_CMD_HCRST)) break;
        for (volatile int i=0;i<1000;i++);
    }
    t = 2000;
    while (t-- > 0) {
        if (!(xhci->op_regs->USBSTS & XHCI_STS_CNR)) break;
        for (volatile int i=0;i<1000;i++);
    }
    klog_printf(KLOG_INFO,"xhci: reset complete");
    return 0;
}

int xhci_reset_port(usb_host_controller_t *hc, uint8_t port) {
    if (!hc || port >= 32) return -1;
    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci || port >= xhci->num_ports) return -1;

    uint32_t p = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
    if (!(p & XHCI_PORTSC_CCS)) return -1;

    uint32_t v = p;
    v &= XHCI_PORTSC_RW_MASK;
    v |= XHCI_PORTSC_PR;
    v &= ~XHCI_PORTSC_PED;
    XHCI_WRITE32(xhci->op_regs, XHCI_PORTSC(port), v);

    uint32_t t = 10000;
    while (t-- > 0) {
        p = XHCI_READ32(xhci->op_regs, XHCI_PORTSC(port));
        if (!(p & XHCI_PORTSC_PR) && (p & XHCI_PORTSC_PED)) {
            klog_printf(KLOG_INFO,"xhci: port %u enabled (PED=1)",port);
            return 0;
        }
        for (volatile int i=0; i<1000; i++);
    }
    return -1;
}

int xhci_transfer_control(usb_host_controller_t *hc, usb_transfer_t *xfer) {
    if (!hc || !xfer) return -1;
    xhci_controller_t *xhci = (xhci_controller_t*)hc->private_data;
    if (!xhci) return -1;
    if (xfer->buffer && xfer->length > 0)
        xhci_flush_cache(xfer->buffer, xfer->length);
<<<<<<< Current (Your changes)
<<<<<<< Current (Your changes)
<<<<<<< Current (Your changes)
    if (xfer->is_control && xfer->setup)
        xhci_flush_cache(xfer->setup, sizeof(*xfer->setup));
=======
    if (xfer->is_control)
        xhci_flush_cache(xfer->setup, sizeof(xfer->setup));
>>>>>>> Incoming (Background Agent changes)
=======
    if (xfer->is_control)
        xhci_flush_cache(xfer->setup, sizeof(xfer->setup));
>>>>>>> Incoming (Background Agent changes)
=======
    if (xfer->is_control)
        xhci_flush_cache(xfer->setup, sizeof(xfer->setup));
>>>>>>> Incoming (Background Agent changes)
    extern int xhci_submit_control_transfer(xhci_controller_t*, usb_transfer_t*);
    return xhci_submit_control_transfer(xhci, xfer);
}

int xhci_transfer_interrupt(usb_host_controller_t *hc, usb_transfer_t *xfer) {
    if (!hc || !xfer || !xfer->endpoint || !xfer->device) return -1;

    xhci_controller_t *xhci = (xhci_controller_t*)hc->private_data;
    if (!xhci) return -1;
    usb_device_t *dev = xfer->device;
    usb_endpoint_t *ep = xfer->endpoint;
    if (!dev || !ep) return -1;
    uint32_t slot = dev->slot_id,
             epn = ep->address & 0xF,
             dir = (ep->address & USB_ENDPOINT_DIR_IN) ? 1 : 0,
             idx = (epn * 2) + dir;
    if (idx < 1 || idx >= 32 || slot > 31) return -1;

    if (!xhci->transfer_rings[slot][idx])
        if (xhci_transfer_ring_init(xhci, slot, idx) < 0) return -1;

    xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][idx];

    if (xfer->buffer && xfer->length > 0)
        xhci_flush_cache(xfer->buffer, xfer->length);

    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    trb.parameter = vmm_virt_to_phys((uint64_t)(uintptr_t)xfer->buffer);

    uint32_t len = xfer->length, flags = XHCI_TRB_IOC;
    if (dir == 0) flags |= XHCI_TRB_ISP;

    trb.status = len | (flags << 16);
    trb.control = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE;

    if (xhci_transfer_ring_enqueue(ring, &trb) < 0) return -1;

    volatile uint32_t *doorbell = (volatile uint32_t*)((uintptr_t)xhci->doorbell_regs + slot*4);
    mmio_write32(doorbell, idx);

    xfer->status = USB_TRANSFER_SUCCESS;
    return 0;
}

int xhci_transfer_bulk(usb_host_controller_t *hc, usb_transfer_t *xfer) {
    if (!hc || !xfer || !xfer->endpoint || !xfer->device) return -1;
    return xhci_transfer_interrupt(hc, xfer);
}

int xhci_transfer_isoc(usb_host_controller_t *hc, usb_transfer_t *xfer) {
    if (!hc || !xfer) return -1;
    xfer->status = USB_TRANSFER_ERROR;
    return -1;
}

int xhci_poll(usb_host_controller_t *hc) {
    if (!hc) return -1;
    xhci_controller_t *xhci = (xhci_controller_t*)hc->private_data;
    extern int xhci_process_events(xhci_controller_t*);
    return xhci_process_events(xhci);
}

void xhci_cleanup(usb_host_controller_t *hc) {
    if (!hc) return;
    xhci_controller_t *xhci = (xhci_controller_t*)hc->private_data;
    if (!xhci) return;
    xhci->op_regs->USBCMD &= ~XHCI_CMD_RUN;

    for (uint32_t slot=0; slot<32; slot++) {
        for (uint32_t ep=0; ep<32; ep++) {
            if (xhci->transfer_rings[slot][ep]) {
                if (xhci->transfer_rings[slot][ep]->trbs)
                    kfree(xhci->transfer_rings[slot][ep]->trbs);
                kfree(xhci->transfer_rings[slot][ep]);
            }
        }
    }
    if (xhci->cmd_ring.trbs) kfree(xhci->cmd_ring.trbs);
    if (xhci->event_ring.trbs) kfree(xhci->event_ring.trbs);
    if (xhci->event_ring.segment_table) kfree(xhci->event_ring.segment_table);
    if (xhci->dcbaap) kfree((void*)xhci->dcbaap);

    if (xhci->scratchpad_buffers) {
        for (uint32_t i=0; i < xhci->scratchpad_size; i++)
            if (xhci->scratchpad_buffers[i]) kfree(xhci->scratchpad_buffers[i]);
        kfree(xhci->scratchpad_buffers);
    }
    kfree(xhci);
    hc->private_data = NULL;
}

/* Nonfunctional PCI probe stub */
int xhci_pci_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    (void)bus; (void)slot; (void)func; return -1;
}

/* Host Controller Operations Table */
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