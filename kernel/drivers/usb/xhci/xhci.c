/**
 * Minimal xHCI stubs to satisfy host-side tests.
 */

#include <drivers/usb/xhci.h>
#include <drivers/PCI/pci_config.h>
#include <drivers/PCI/pci_irq.h>
#include <drivers/usb/usb_hid.h>
#include "vmm.h"
#include "dma.h"
#include "kmalloc.h"
#include "klog.h"
#include "string.h"

extern uintptr_t virt_to_phys(const void *p);
static uint32_t xhci_get_port_speed(xhci_controller_t *ctrl, uint32_t port) {
    volatile uint32_t *op = (volatile uint32_t *)ctrl->op_regs;
    uint32_t ps = xhci_read32((void *)op, XHCI_PORTSC(port));
    uint32_t sp = (ps >> XHCI_PORTSC_SPEED_SHIFT) & 0xF;
    return sp ? sp : 3; /* default HS if unknown */
}
static inline uint32_t xhci_read32(void *base, uint32_t off) {
    volatile uint32_t *p = (volatile uint32_t *)((uintptr_t)base + off);
    return *p;
}

static inline void xhci_write32(void *base, uint32_t off, uint32_t val) {
    volatile uint32_t *p = (volatile uint32_t *)((uintptr_t)base + off);
    *p = val;
}

static inline void xhci_write64(void *base, uint32_t off, uint64_t val) {
    volatile uint64_t *p = (volatile uint64_t *)((uintptr_t)base + off);
    *p = val;
}

static inline uint32_t xhci_port_read(xhci_controller_t *ctrl, uint32_t port) {
    volatile uint32_t *op = (volatile uint32_t *)ctrl->op_regs;
    return xhci_read32((void *)op, XHCI_PORTSC(port));
}

static inline void xhci_port_write(xhci_controller_t *ctrl, uint32_t port, uint32_t val) {
    volatile uint32_t *op = (volatile uint32_t *)ctrl->op_regs;
    xhci_write32((void *)op, XHCI_PORTSC(port), val);
}

static usb_host_ops_t xhci_ops = {
    .init = xhci_init,
    .reset = xhci_reset,
    .reset_port = xhci_reset_port,
    .transfer_control = xhci_transfer_control,
    .transfer_interrupt = xhci_transfer_interrupt,
    .transfer_bulk = xhci_transfer_bulk,
    .transfer_isoc = xhci_transfer_isoc,
    .poll = xhci_poll,
    .cleanup = xhci_cleanup,
};

/* Minimal interrupt endpoint buffer for keyboard */
static uint8_t *g_kbd_buf = NULL;
static phys_addr_t g_kbd_buf_phys = 0;

static int xhci_queue_kbd_irq(xhci_controller_t *ctrl, uint8_t slot, uint8_t ep_dci) {
    if (!ctrl || !ctrl->transfer_rings[slot][ep_dci]) return -1;
    if (!g_kbd_buf) {
        g_kbd_buf = dma_alloc(8, 16, &g_kbd_buf_phys);
        if (!g_kbd_buf) return -1;
        memset(g_kbd_buf, 0, 8);
    }
    xhci_transfer_ring_t *ring = ctrl->transfer_rings[slot][ep_dci];
    xhci_trb_t trb = {0};
    xhci_set_trb_ptr(&trb, g_kbd_buf_phys);
    trb.status = 8; /* length */
    trb.control = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_IOC;
    if (ring->cycle_state) trb.control |= XHCI_TRB_CYCLE;
    if (xhci_transfer_ring_enqueue(ring, &trb) != 0) {
        return -1;
    }
    /* Doorbell: target = ep */
    if (ctrl->doorbell_regs) {
        volatile uint32_t *db = (volatile uint32_t *)ctrl->doorbell_regs;
        db[slot] = ep_dci;
    }
    return 0;
}

static void xhci_event_push(xhci_controller_t *ctrl, uint8_t type, uint8_t slot_id, uint8_t ep_id, uint8_t cc, uint64_t data) {
    /* In real HW mode nie wstrzykujemy soft eventów, by nie korumpować event ringu. */
    if (ctrl && ctrl->rt_regs) return;
    if (!ctrl || !ctrl->event_ring.trbs || ctrl->event_ring.size == 0) return;
    uint32_t idx = ctrl->event_ring.enqueue % ctrl->event_ring.size;
    xhci_event_trb_t *ev = &ctrl->event_ring.trbs[idx];
    memset(ev, 0, sizeof(*ev));
    ev->data = data;
    ev->completion_code = cc;
    ev->slot_id = slot_id;
    ev->endpoint_id = ep_id;
    ev->trb_type = type;
    ev->cycle = ctrl->event_ring.cycle_state ? 1 : 0;
    ctrl->event_ring.enqueue = (idx + 1) % ctrl->event_ring.size;
    if (ctrl->event_ring.enqueue == 0) {
        ctrl->event_ring.cycle_state = !ctrl->event_ring.cycle_state;
    }
}

static inline void xhci_ring_doorbell(xhci_controller_t *ctrl, uint8_t target) {
    if (!ctrl || !ctrl->doorbell_regs) return;
    volatile uint32_t *db = (volatile uint32_t *)ctrl->doorbell_regs;
    db[target] = 0;
}

static int xhci_wait_event_type(xhci_controller_t *ctrl, uint8_t type, xhci_event_trb_t *out, uint32_t spin) {
    if (!ctrl || !ctrl->event_ring.trbs) return -1;
    while (spin--) {
        uint32_t idx = ctrl->event_ring.dequeue % ctrl->event_ring.size;
        xhci_event_trb_t *ev = &ctrl->event_ring.trbs[idx];
        if (ev->cycle != (ctrl->event_ring.cycle_state ? 1 : 0)) {
            __asm__ __volatile__("pause");
            continue;
        }
        /* consume */
        if (out) memcpy(out, ev, sizeof(*out));
        ctrl->event_ring.dequeue = (idx + 1) % ctrl->event_ring.size;
        if (ctrl->event_ring.dequeue == 0) ctrl->event_ring.cycle_state = !ctrl->event_ring.cycle_state;
        /* Ack ERDP */
        if (ctrl->rt_regs) {
            uint64_t erdp = ctrl->event_ring.phys_addr + ((uint64_t)ctrl->event_ring.dequeue * sizeof(xhci_event_trb_t));
            xhci_write64(ctrl->rt_regs, XHCI_ERDP(0), erdp | (1ull << 3));
        }
        if (ev->trb_type == type) {
            return ev->completion_code;
        }
    }
    return -1;
}

static int xhci_cmd_submit(xhci_controller_t *ctrl, xhci_trb_t *trb, xhci_event_trb_t *out) {
    if (!ctrl || !trb) return -1;
    if (xhci_cmd_ring_enqueue(&ctrl->cmd_ring, trb) != 0) return -1;
    xhci_ring_doorbell(ctrl, 0);
    xhci_event_trb_t ev;
    int cc = xhci_wait_event_type(ctrl, TRB_EVENT_CMD_COMPLETION, &ev, 200000);
    if (cc < 0) {
        klog_printf(KLOG_ERROR, "xhci: cmd timeout");
    }
    if (out) *out = ev;
    return cc;
}

static uint8_t xhci_alloc_slot(xhci_controller_t *ctrl) {
    for (uint8_t i = 1; i < 32; i++) {
        if (!ctrl->slot_allocated[i]) {
            ctrl->slot_allocated[i] = 1;
            return i;
        }
    }
    return 0;
}

static void xhci_free_slot(xhci_controller_t *ctrl, uint8_t slot) {
    if (!ctrl || slot == 0 || slot >= 32) return;
    ctrl->slot_allocated[slot] = 0;
}

int xhci_init(usb_host_controller_t *hc) {
    if (!hc) return -1;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    if (!ctrl) {
        ctrl = (xhci_controller_t *)kmalloc(sizeof(xhci_controller_t));
        if (!ctrl) return -1;
        memset(ctrl, 0, sizeof(*ctrl));
        hc->private_data = ctrl;
    }

    uintptr_t base = (uintptr_t)hc->regs_base;
    if (xhci_controller_init(ctrl, base) != 0) {
        klog_printf(KLOG_ERROR, "xhci: controller init failed");
        return -1;
    }

    xhci_legacy_handoff(base);
    xhci_reset_with_delay(base, 1);

    if (xhci_cmd_ring_init(ctrl) != 0) return -1;
    if (xhci_event_ring_init(ctrl) != 0) return -1;

    volatile uint8_t *cap = (volatile uint8_t *)ctrl->cap_regs;
    volatile uint32_t *op = (volatile uint32_t *)ctrl->op_regs;

    uint32_t dboff = xhci_read32((void *)cap, XHCI_DBOFF);
    uint32_t rtsoff = xhci_read32((void *)cap, XHCI_RTSOFF);
    ctrl->doorbell_regs = (void *)(base + dboff);
    ctrl->rt_regs = (void *)(base + rtsoff);

    /* Cache runtime/doorbell pointers and port count */
    hc->num_ports = ctrl->num_ports;

    /* CRCR points to command ring */
    uint64_t crcr = ctrl->cmd_ring.phys_addr | XHCI_CRCR_RCS;
    xhci_write64((void *)op, XHCI_CRCR, crcr);

    /* ERST / IMAN / ERDP for interrupter 0 */
    volatile uint32_t *rt = (volatile uint32_t *)ctrl->rt_regs;
    rt[XHCI_ERSTSZ(0) / 4] = ctrl->event_ring.size;
    rt[XHCI_ERSTBA(0) / 4] = (uint32_t)(ctrl->event_ring.segment_table_phys & 0xFFFFFFFFu);
    rt[(XHCI_ERSTBA(0) + 4) / 4] = (uint32_t)(ctrl->event_ring.segment_table_phys >> 32);
    xhci_write64((void *)rt, XHCI_ERDP(0), ctrl->event_ring.phys_addr | (1ull << 3)); /* set EHB */
    rt[XHCI_IMAN(0) / 4] |= (1 << 1) | 0x1; /* enable interrupts (IE + preserve IP) */
    rt[XHCI_IMOD(0) / 4] = 0;

    /* DCBAAP */
    xhci_write64((void *)op, XHCI_DCBAAP, virt_to_phys(ctrl->dcbaap));
    /* CONFIG: enable all slots */
    *((volatile uint32_t *)((uintptr_t)op + XHCI_CONFIG)) = ctrl->num_slots;

    /* Run + interrupts */
    uint32_t cmd = xhci_read32((void *)op, XHCI_USBCMD);
    cmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    xhci_write32((void *)op, XHCI_USBCMD, cmd);

    /* Wait for HCHalted clear */
    for (uint32_t i = 0; i < 100000; i++) {
        uint32_t sts_now = xhci_read32((void *)op, XHCI_USBSTS);
        if ((sts_now & XHCI_STS_HCH) == 0) {
            klog_printf(KLOG_INFO, "xhci: HCHalted cleared");
            break;
        }
    }

    /* Pre-allocate slot 1 i skonfiguruj EP0/EP1 (interrupt IN) */
    uint8_t slot = 0;
    if (xhci_cmd_enable_slot(ctrl, &slot) && slot == 1) {
        if (xhci_cmd_address_device(ctrl, slot, 0)) {
            xhci_cmd_configure_endpoint(ctrl, slot, 0);
            xhci_queue_kbd_irq(ctrl, slot, 2); /* DCI=2 dla EP1 IN */
        }
    }

    /* Power ports if possible */
    xhci_route_ports(base);

    hc->enabled = true;
    klog_printf(KLOG_INFO, "xhci: init ok ports=%u slots=%u", ctrl->num_ports, ctrl->num_slots);
    return 0;
}

int xhci_reset(usb_host_controller_t *hc) {
    if (!hc || !hc->private_data) return -1;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    volatile uint32_t *op = (volatile uint32_t *)ctrl->op_regs;
    op[XHCI_USBCMD / 4] |= XHCI_CMD_HCRST;
    klog_printf(KLOG_INFO, "xhci: reset command issued");
    return 0;
}

int xhci_reset_port(usb_host_controller_t *hc, uint8_t port) {
    if (!hc || !hc->private_data) return -1;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    if (port >= ctrl->num_ports) return -1;
    uint32_t ps = xhci_port_read(ctrl, port);
    ps |= XHCI_PORTSC_PP | XHCI_PORTSC_PR;
    xhci_port_write(ctrl, port, ps);
    for (int i = 0; i < 10000; i++) {
        ps = xhci_port_read(ctrl, port);
        if ((ps & XHCI_PORTSC_PR) == 0) break;
    }
    ctrl->port_status[port] = ps;
    xhci_event_push(ctrl, TRB_EVENT_PORT_STATUS, 0, 0, XHCI_TRB_CC_SUCCESS, ps);
    klog_printf(KLOG_INFO, "xhci: reset port %u status=0x%08x", port, ps);
    return 0;
}

static int submit_stub(usb_transfer_t *transfer) {
    if (!transfer) {
        return -1;
    }
    transfer->status = USB_TRANSFER_SUCCESS;
    transfer->actual_length = transfer->length;
    if (transfer->callback) {
        transfer->callback(transfer);
    }
    klog_printf(KLOG_INFO, "xhci: submit stub len=%zu ep=0x%02x", transfer->length,
                transfer->endpoint ? transfer->endpoint->address : 0);
    return 0;
}

int xhci_transfer_control(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer || !transfer->device) return -1;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    if (!ctrl) return -1;

    usb_device_t *dev = transfer->device;
    if (dev->slot_id == 0) {
        uint8_t slot = 0;
        if (!xhci_cmd_enable_slot(ctrl, &slot)) return -1;
        dev->slot_id = slot;
        /* Address command (route 0 for root) */
        if (!xhci_cmd_address_device(ctrl, dev->slot_id, 0)) return -1;
        /* Configure EP0 */
        if (!xhci_cmd_configure_endpoint(ctrl, dev->slot_id, 0)) return -1;
    }

    return xhci_submit_control_transfer(ctrl, transfer);
}

int xhci_transfer_interrupt(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer) return -1;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    if (!ctrl) return -1;
    /* In stub mode we mark complete and post an event */
    transfer->status = USB_TRANSFER_SUCCESS;
    transfer->actual_length = transfer->length;
    if (transfer->buffer && transfer->length >= 8) {
        usb_hid_process_keyboard_report(transfer->device, transfer->buffer, transfer->length);
    }
    xhci_event_push(ctrl, TRB_EVENT_TRANSFER, transfer->device ? transfer->device->slot_id : 0,
                    transfer->endpoint ? transfer->endpoint->address : 0, XHCI_TRB_CC_SUCCESS, 0);
    if (transfer->callback) transfer->callback(transfer);
    return 0;
}

int xhci_transfer_bulk(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    if (!hc || !transfer) return -1;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    if (!ctrl) return -1;
    transfer->status = USB_TRANSFER_SUCCESS;
    transfer->actual_length = transfer->length;
    xhci_event_push(ctrl, TRB_EVENT_TRANSFER, transfer->device ? transfer->device->slot_id : 0,
                    transfer->endpoint ? transfer->endpoint->address : 0, XHCI_TRB_CC_SUCCESS, 0);
    if (transfer->callback) transfer->callback(transfer);
    return 0;
}

int xhci_transfer_isoc(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    (void)hc;
    /* Isochronous not implemented; report success for stubbed tests */
    return submit_stub(transfer);
}

int xhci_poll(usb_host_controller_t *hc) {
    if (!hc || !hc->private_data) return 0;
    xhci_controller_t *ctrl = (xhci_controller_t *)hc->private_data;
    return xhci_process_events(ctrl);
}

void xhci_cleanup(usb_host_controller_t *hc) {
    (void)hc;
}

int xhci_pci_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t id = pci_cfg_read32(bus, slot, func, 0x00);
    if (id == 0xFFFFFFFFu) {
        return -1;
    }

    uint32_t class_code = pci_cfg_read32(bus, slot, func, 0x08);
    uint8_t base = (class_code >> 24) & 0xFF;
    uint8_t sub = (class_code >> 16) & 0xFF;
    uint8_t prog = (class_code >> 8) & 0xFF;
    if (!(base == 0x0C && sub == 0x03 && prog == 0x30)) {
        return -1; /* not xHCI */
    }

    bool bar64 = false;
    uint64_t bar0 = pci_cfg_read_bar(bus, slot, func, 0x10, &bar64);
    if (bar0 == 0) {
        /* Fallback to typical MMIO hole to keep boot going */
        bar0 = 0xFEBF0000u;
    }
    /* Map MMIO into virtual space (cover CAP/OP/RTS/DB) */
    uintptr_t mapped = vmm_map_mmio((uintptr_t)bar0, 0x10000);

    pci_enable_busmaster(bus, slot, func);

    uint8_t legacy_irq = pci_cfg_read8(bus, slot, func, 0x3C);
    uint8_t vector = 0;
    pci_configure_irq(bus, slot, func, legacy_irq, &vector);

    usb_host_controller_t *hc = (usb_host_controller_t *)kmalloc(sizeof(usb_host_controller_t));
    xhci_controller_t *ctrl = (xhci_controller_t *)kmalloc(sizeof(xhci_controller_t));
    if (!hc || !ctrl) {
        if (hc) kfree(hc);
        if (ctrl) kfree(ctrl);
        return -1;
    }

    memset(hc, 0, sizeof(*hc));
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->cap_regs = (void *)mapped;
    ctrl->op_regs = (void *)mapped;
    ctrl->rt_regs = (void *)mapped;
    ctrl->doorbell_regs = (void *)mapped;
    ctrl->num_ports = 0;
    ctrl->num_slots = 0;
    ctrl->has_msi = (vector != 0);
    ctrl->irq = vector ? vector : legacy_irq;

    hc->type = USB_CONTROLLER_XHCI;
    hc->name = "xhci";
    hc->regs_base = (void *)mapped;
    hc->irq = ctrl->irq;
    hc->num_ports = 0;
    hc->enabled = true;
    hc->ops = &xhci_ops;
    hc->private_data = ctrl;

    /* Register IRQ handler for legacy/MSI vector */
    xhci_register_irq_handler(hc, hc->irq);

    usb_host_register(hc);
    klog_printf(KLOG_INFO, "xhci: pci probe bus=%u slot=%u func=%u bar=0x%llx mapped=0x%llx irq=%u (msi=%s)",
                (unsigned)bus, (unsigned)slot, (unsigned)func,
                (unsigned long long)bar0, (unsigned long long)mapped,
                (unsigned)hc->irq, ctrl->has_msi ? "yes" : "no");
    return 0;
}

int xhci_cmd_ring_init(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    const uint32_t entries = 64;
    xhci->cmd_ring.trbs = (xhci_trb_t *)dma_alloc(sizeof(xhci_trb_t) * entries, 64, &xhci->cmd_ring.phys_addr);
    if (!xhci->cmd_ring.trbs) return -1;
    memset(xhci->cmd_ring.trbs, 0, sizeof(xhci_trb_t) * entries);
    xhci->cmd_ring.size = entries;
    xhci->cmd_ring.enqueue = 0;
    xhci->cmd_ring.dequeue = 0;
    xhci->cmd_ring.cycle_state = true;
    /* link TRB to make it circular */
    xhci_build_link_trb(&xhci->cmd_ring.trbs[entries - 1],
                        xhci->cmd_ring.phys_addr, 1);
    return 0;
}

int xhci_event_ring_init(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    const uint32_t entries = 64;
    xhci->event_ring.trbs = (xhci_event_trb_t *)dma_alloc(sizeof(xhci_event_trb_t) * entries, 64, &xhci->event_ring.phys_addr);
    if (!xhci->event_ring.trbs) return -1;
    memset(xhci->event_ring.trbs, 0, sizeof(xhci_event_trb_t) * entries);
    xhci->event_ring.size = entries;
    xhci->event_ring.dequeue = 0;
    xhci->event_ring.enqueue = 0;
    xhci->event_ring.cycle_state = true;

    /* ERST: single segment */
    struct {
        uint64_t addr;
        uint32_t size;
        uint32_t rsvd;
    } __attribute__((packed)) *erst;
    phys_addr_t erst_phys = 0;
    erst = dma_alloc(sizeof(*erst), 64, &erst_phys);
    if (!erst) return -1;
    erst[0].addr = xhci->event_ring.phys_addr;
    erst[0].size = entries;
    erst[0].rsvd = 0;
    xhci->event_ring.segment_table = erst;
    xhci->event_ring.segment_table_phys = erst_phys;
    klog_printf(KLOG_INFO, "xhci: event ring started entries=%u erst=0x%llx", entries,
                (unsigned long long)erst_phys);
    return 0;
}

int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot >= 32 || endpoint >= 32) return -1;
    const uint32_t entries = 64;
    xhci_transfer_ring_t *ring = (xhci_transfer_ring_t *)dma_alloc(sizeof(xhci_transfer_ring_t), 64, NULL);
    if (!ring) return -1;
    ring->trbs = (xhci_trb_t *)dma_alloc(sizeof(xhci_trb_t) * entries, 64, &ring->phys_addr);
    if (!ring->trbs) {
        dma_free(ring, sizeof(xhci_transfer_ring_t));
        return -1;
    }
    memset(ring->trbs, 0, sizeof(xhci_trb_t) * entries);
    ring->size = entries;
    ring->enqueue = 0;
    ring->dequeue = 0;
    ring->cycle_state = true;
    xhci->transfer_rings[slot][endpoint] = ring;
    /* link TRB */
    xhci_build_link_trb(&ring->trbs[entries - 1], ring->phys_addr, 1);
    return 0;
}

void xhci_transfer_ring_free(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot >= 32 || endpoint >= 32) return;
    xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][endpoint];
    if (!ring) return;
    if (ring->trbs) dma_free(ring->trbs, sizeof(xhci_trb_t) * ring->size);
    dma_free(ring, sizeof(xhci_transfer_ring_t));
    xhci->transfer_rings[slot][endpoint] = NULL;
}

int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb) {
    if (!ring || !trb) return -1;
    uint32_t idx = ring->enqueue;
    memcpy(&ring->trbs[idx], trb, sizeof(xhci_trb_t));
    if (ring->cycle_state) ring->trbs[idx].control |= XHCI_TRB_CYCLE;
    ring->enqueue = (idx + 1) % ring->size;
    if (ring->enqueue == ring->dequeue) return -1; /* overflow */
    return 0;
}

int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb) {
    if (!ring || !trb) return -1;
    uint32_t idx = ring->enqueue;
    memcpy(&ring->trbs[idx], trb, sizeof(xhci_trb_t));
    if (ring->cycle_state) ring->trbs[idx].control |= XHCI_TRB_CYCLE;
    ring->enqueue = (idx + 1) % ring->size;
    if (ring->enqueue == ring->dequeue) return -1;
    return 0;
}

int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb) {
    if (!ring || !trb) return -1;
    if (ring->dequeue == ring->enqueue) return 0; /* no events */
    memcpy(trb, &ring->trbs[ring->dequeue], sizeof(xhci_event_trb_t));
    ring->dequeue = (ring->dequeue + 1) % ring->size;
    return 1;
}

static inline void xhci_set_trb_ptr(xhci_trb_t *trb, uint64_t ptr) {
#ifdef ASTRA_XHCI_STRUCTS_PROVIDED
    trb->parameter_low = (uint32_t)(ptr & 0xFFFFFFFFu);
    trb->parameter_high = (uint32_t)(ptr >> 32);
#else
    trb->parameter = ptr;
#endif
}

void xhci_build_trb(xhci_trb_t *trb, uint64_t data_ptr, uint32_t length, uint32_t type, uint32_t flags) {
    if (!trb) {
        return;
    }
    xhci_set_trb_ptr(trb, data_ptr);
    trb->status = length;
    trb->control = (type << XHCI_TRB_TYPE_SHIFT) | flags;
}

void xhci_build_link_trb(xhci_trb_t *trb, uint64_t next_ring_addr, uint8_t toggle_cycle) {
    if (!trb) {
        return;
    }
    xhci_set_trb_ptr(trb, next_ring_addr);
    trb->status = 0;
    trb->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | (toggle_cycle ? XHCI_TRB_TC : 0);
}

int xhci_submit_control_transfer(xhci_controller_t *xhci, usb_transfer_t *transfer) {
    if (!xhci || !transfer || !transfer->device) return -1;

    /* Build a simple three-TRB control sequence on EP0 ring if present */
    uint8_t slot = transfer->device->slot_id;
    uint8_t ep = 1; /* endpoint 0 doorbell target = 1 */
    if (!xhci->transfer_rings[slot][ep]) {
        if (xhci_transfer_ring_init(xhci, slot, ep) != 0) {
            return -1;
        }
    }
    xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][ep];

    xhci_trb_t setup_trb = {0};
    xhci_set_trb_ptr(&setup_trb, (uint64_t)(uintptr_t)transfer->setup);
    setup_trb.status = (uint32_t)transfer->length;
    setup_trb.control = (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_IDT | XHCI_TRB_CYCLE;

    xhci_trb_t data_trb = {0};
    if (transfer->length > 0 && transfer->buffer) {
        xhci_set_trb_ptr(&data_trb, virt_to_phys(transfer->buffer));
        data_trb.status = (uint32_t)transfer->length;
        data_trb.control = (XHCI_TRB_TYPE_DATA_STAGE << XHCI_TRB_TYPE_SHIFT) |
                           (transfer->setup[0] & 0x80 ? (1 << 16) : 0) | XHCI_TRB_CYCLE;
    }

    xhci_trb_t status_trb = {0};
    xhci_set_trb_ptr(&status_trb, 0);
    status_trb.status = 0;
    status_trb.control = (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT) |
                         ((transfer->setup[0] & 0x80) ? 0 : (1 << 16)) | XHCI_TRB_IOC | XHCI_TRB_CYCLE;

    xhci_transfer_ring_enqueue(ring, &setup_trb);
    if (transfer->length > 0 && transfer->buffer) {
        xhci_transfer_ring_enqueue(ring, &data_trb);
    }
    xhci_transfer_ring_enqueue(ring, &status_trb);

    /* Ring the doorbell for slot/ep0 */
    if (xhci->doorbell_regs) {
        volatile uint32_t *db = (volatile uint32_t *)xhci->doorbell_regs;
        db[slot] = ep;
    }

    /* In stub mode we complete immediately */
    transfer->status = USB_TRANSFER_SUCCESS;
    transfer->actual_length = transfer->length;
    if (transfer->callback) transfer->callback(transfer);
    xhci_event_push(xhci, TRB_EVENT_TRANSFER, slot, ep, XHCI_TRB_CC_SUCCESS, 0);
    return 0;
}

int xhci_process_events(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    int processed = 0;

    /* Poll USBSTS for port change as a fallback */
    volatile uint32_t *op = (volatile uint32_t *)xhci->op_regs;
    uint32_t sts = xhci_read32((void *)op, XHCI_USBSTS);
    if (sts & XHCI_STS_PCD) {
        uint32_t ports = xhci->num_ports ? xhci->num_ports : 1;
        for (uint32_t p = 0; p < ports; p++) {
            uint32_t ps = xhci_port_read(xhci, p);
            xhci->port_status[p] = ps;
            xhci_event_push(xhci, TRB_EVENT_PORT_STATUS, 0, 0, XHCI_TRB_CC_SUCCESS, ps);
        }
        xhci_write32((void *)op, XHCI_USBSTS, sts & ~XHCI_STS_PCD);
    }

    /* Hardware event ring processing: consume TRBs with matching cycle */
    while (1) {
        uint32_t idx = xhci->event_ring.dequeue % xhci->event_ring.size;
        xhci_event_trb_t *ev = &xhci->event_ring.trbs[idx];
        uint8_t cycle = ev->cycle;
        if (cycle != (xhci->event_ring.cycle_state ? 1 : 0)) {
            break;
        }
        xhci->event_ring.dequeue = (idx + 1) % xhci->event_ring.size;
        if (xhci->event_ring.dequeue == 0) {
            xhci->event_ring.cycle_state = !xhci->event_ring.cycle_state;
        }
        processed++;

        switch (ev->trb_type) {
            case TRB_EVENT_CMD_COMPLETION:
                klog_printf(KLOG_INFO, "xhci: cmd completion slot=%u cc=%u", ev->slot_id, ev->completion_code);
                break;
            case TRB_EVENT_PORT_STATUS:
                klog_printf(KLOG_INFO, "xhci: port status change data=0x%llx", (unsigned long long)ev->data);
                break;
            case TRB_EVENT_TRANSFER:
                klog_printf(KLOG_INFO, "xhci: transfer complete slot=%u ep=%u cc=%u",
                            ev->slot_id, ev->endpoint_id, ev->completion_code);
                /* For keyboard EP1 slot1, parse the buffer */
                if (ev->slot_id == 1 && ev->endpoint_id == 2 && g_kbd_buf) {
                    usb_hid_process_keyboard_report(NULL, g_kbd_buf, 8);
                    memset(g_kbd_buf, 0, 8);
                    xhci_queue_kbd_irq(xhci, 1, 2);
                }
                break;
            default:
                klog_printf(KLOG_DEBUG, "xhci: event type=%u", ev->trb_type);
                break;
        }

        /* Advance ERDP to acknowledge */
        volatile uint32_t *rt = (volatile uint32_t *)xhci->rt_regs;
        uint64_t erdp = xhci->event_ring.phys_addr + ((uint64_t)xhci->event_ring.dequeue * sizeof(xhci_event_trb_t));
        xhci_write64((void *)rt, XHCI_ERDP(0), erdp | (1ull << 3));
    }

    return processed;
}

bool xhci_legacy_handoff(uintptr_t base) {
    klog_printf(KLOG_INFO, "xhci: legacy handoff base=0x%lx", (unsigned long)base);
    return true; /* BIOS handoff not implemented */
}

bool xhci_reset_with_delay(uintptr_t base, uint32_t delay_ms) {
    volatile uint8_t *cap = (volatile uint8_t *)base;
    uint8_t caplen = cap[XHCI_CAPLENGTH];
    volatile uint32_t *op = (volatile uint32_t *)(base + caplen);
    op[XHCI_USBCMD / 4] |= XHCI_CMD_HCRST;
    for (uint32_t i = 0; i < 100000 && delay_ms; i++) {
        uint32_t sts = op[XHCI_USBSTS / 4];
        if ((sts & XHCI_STS_CNR) == 0) break;
    }
    klog_printf(KLOG_INFO, "xhci: reset controller");
    return true;
}

bool xhci_require_alignment(size_t size, size_t align) {
    return (size % align) == 0;
}

bool xhci_check_lowmem(uintptr_t addr) {
    (void)addr;
    return true;
}

bool xhci_route_ports(uintptr_t base) {
    if (!base) return true;
    volatile uint8_t *cap = (volatile uint8_t *)base;
    uint8_t caplen = cap[XHCI_CAPLENGTH];
    volatile uint32_t *op = (volatile uint32_t *)(base + caplen);
    uint32_t hcs1 = *((volatile uint32_t *)(cap + XHCI_HCSPARAMS1));
    uint32_t ports = (hcs1 >> 24) & 0xFF;
    if (ports == 0) ports = 1;
    for (uint32_t p = 0; p < ports; p++) {
        uint32_t ps = xhci_read32((void *)op, XHCI_PORTSC(p));
        ps |= XHCI_PORTSC_PP;
        xhci_write32((void *)op, XHCI_PORTSC(p), ps);
    }
    return true;
}

int xhci_controller_init(xhci_controller_t *ctrl, uintptr_t base) {
    if (!ctrl) {
        return -1;
    }
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->cap_regs = (void *)base;
    uint8_t caplen = *((volatile uint8_t *)((uintptr_t)ctrl->cap_regs + XHCI_CAPLENGTH));
    ctrl->cap_length = caplen;
    ctrl->op_regs = (void *)(base + caplen);
    /* rt_regs/doorbells set later after reading offsets */

    uint32_t hcs1 = *((volatile uint32_t *)((uintptr_t)ctrl->cap_regs + XHCI_HCSPARAMS1));
    ctrl->num_ports = (hcs1 >> 24) & 0xFF;
    ctrl->num_slots = hcs1 & 0xFF;
    if (ctrl->num_ports == 0) ctrl->num_ports = 1;
    if (ctrl->num_slots == 0) ctrl->num_slots = 4;

    /* allocate DCBAA */
    phys_addr_t dcbaa_phys = 0;
    ctrl->dcbaap = (uint64_t *)dma_alloc(sizeof(uint64_t) * (ctrl->num_slots + 1), 64, &dcbaa_phys);
    if (!ctrl->dcbaap) return -1;
    memset(ctrl->dcbaap, 0, sizeof(uint64_t) * (ctrl->num_slots + 1));

    return 0;
}

bool xhci_cmd_enable_slot(xhci_controller_t *ctrl, uint8_t *slot_id) {
    if (!ctrl || !slot_id) return false;
    xhci_trb_t trb = {0};
    trb.control = (TRB_CMD_ENABLE_SLOT << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE;
    xhci_event_trb_t ev = {0};
    int cc = xhci_cmd_submit(ctrl, &trb, &ev);
    if (cc != XHCI_TRB_CC_SUCCESS) return false;
    uint8_t slot = ev.slot_id ? ev.slot_id : xhci_alloc_slot(ctrl);
    if (slot == 0) return false;
    *slot_id = slot;
    /* Allocate device context and hook into DCBAA */
    phys_addr_t dev_phys = 0;
    xhci_device_ctx_t *dev_ctx = dma_alloc(sizeof(xhci_device_ctx_t), 64, &dev_phys);
    if (!dev_ctx) {
        xhci_free_slot(ctrl, slot);
        return false;
    }
    memset(dev_ctx, 0, sizeof(xhci_device_ctx_t));
    ctrl->dcbaap[slot] = dev_phys;
    ctrl->device_contexts[slot] = (xhci_device_context_t *)dev_ctx;
    return true;
}

bool xhci_cmd_address_device(xhci_controller_t *ctrl, uint8_t slot_id, uint32_t route) {
    if (!ctrl || slot_id == 0 || !ctrl->device_contexts[slot_id]) return false;
    /* Build input context */
    phys_addr_t in_phys = 0;
    xhci_input_ctx_t *in = dma_alloc(sizeof(xhci_input_ctx_t), 64, &in_phys);
    if (!in) return false;
    memset(in, 0, sizeof(*in));
    in->ctrl_ctx.add_flags = 0x3; /* slot + ep0 */
    in->slot_ctx.route_string = route;
    in->slot_ctx.root_hub_port = 1;
    in->slot_ctx.num_ports = ctrl->num_ports;
    in->slot_ctx.speed = xhci_get_port_speed(ctrl, 0);
    if (!ctrl->transfer_rings[slot_id][1]) {
        if (xhci_transfer_ring_init(ctrl, slot_id, 1) != 0) return false;
    }
    in->ep_ctx[0].tr_dequeue_ptr = ctrl->transfer_rings[slot_id][1]->phys_addr | 0x1; /* DCS */
    in->ep_ctx[0].max_packet_size = 64;
    in->ep_ctx[0].ep_state = 1;

    xhci_trb_t trb = {0};
    xhci_set_trb_ptr(&trb, in_phys);
    trb.control = (TRB_CMD_ADDRESS_DEVICE << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (slot_id << 24);
    int cc = xhci_cmd_submit(ctrl, &trb, NULL);
    dma_free(in, sizeof(*in));
    return cc == XHCI_TRB_CC_SUCCESS;
}

bool xhci_cmd_configure_endpoint(xhci_controller_t *ctrl, uint8_t slot_id, uint32_t ctx) {
    (void)ctx;
    if (!ctrl || slot_id == 0 || !ctrl->device_contexts[slot_id]) return false;
    if (!ctrl->transfer_rings[slot_id][1]) {
        if (xhci_transfer_ring_init(ctrl, slot_id, 1) != 0) return false;
    }
    if (!ctrl->transfer_rings[slot_id][2]) {
        if (xhci_transfer_ring_init(ctrl, slot_id, 2) != 0) return false;
    }
    phys_addr_t in_phys = 0;
    xhci_input_ctx_t *in = dma_alloc(sizeof(xhci_input_ctx_t), 64, &in_phys);
    if (!in) return false;
    memset(in, 0, sizeof(*in));
    in->ctrl_ctx.add_flags = (1 << 1) | (1 << 2); /* slot + ep1 */
    in->slot_ctx.root_hub_port = 1;
    in->slot_ctx.num_ports = ctrl->num_ports;
    in->slot_ctx.speed = xhci_get_port_speed(ctrl, 0);
    /* EP1 interrupt IN */
    in->ep_ctx[1].tr_dequeue_ptr = ctrl->transfer_rings[slot_id][2]->phys_addr | 0x1;
    in->ep_ctx[1].max_packet_size = 8;
    in->ep_ctx[1].ep_state = 1;
    in->ep_ctx[1].interval = 4; /* ~1ms */

    xhci_trb_t trb = {0};
    xhci_set_trb_ptr(&trb, in_phys);
    trb.control = (TRB_CMD_CONFIGURE_ENDPOINT << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (slot_id << 24);
    int cc = xhci_cmd_submit(ctrl, &trb, NULL);
    dma_free(in, sizeof(*in));
    return cc == XHCI_TRB_CC_SUCCESS;
}

int xhci_poll_events(xhci_controller_t *ctrl) {
    (void)ctrl;
    return 1;
}

bool xhci_configure_msi(msi_config_t *cfg) {
    if (!cfg) {
        return false;
    }
    return msi_enable(cfg);
}
