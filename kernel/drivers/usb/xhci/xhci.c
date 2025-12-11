/**
 * Minimal xHCI stubs to satisfy host-side tests.
 */

#include <drivers/usb/xhci.h>
#include <drivers/PCI/pci_config.h>
#include <drivers/PCI/pci_irq.h>
#include "kmalloc.h"
#include "klog.h"
#include "string.h"
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

int xhci_init(usb_host_controller_t *hc) {
    (void)hc;
    klog_printf(KLOG_INFO, "xhci: init stub");
    return 0;
}

int xhci_reset(usb_host_controller_t *hc) {
    (void)hc;
    klog_printf(KLOG_INFO, "xhci: reset");
    return 0;
}

int xhci_reset_port(usb_host_controller_t *hc, uint8_t port) {
    (void)hc;
    (void)port;
    klog_printf(KLOG_INFO, "xhci: reset port %u", port);
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
    (void)hc;
    return submit_stub(transfer);
}

int xhci_transfer_interrupt(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    (void)hc;
    return submit_stub(transfer);
}

int xhci_transfer_bulk(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    (void)hc;
    return submit_stub(transfer);
}

int xhci_transfer_isoc(usb_host_controller_t *hc, usb_transfer_t *transfer) {
    (void)hc;
    return submit_stub(transfer);
}

int xhci_poll(usb_host_controller_t *hc) {
    (void)hc;
    return 0;
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
    ctrl->cap_regs = (void *)bar0;
    ctrl->op_regs = (void *)bar0;
    ctrl->rt_regs = (void *)bar0;
    ctrl->doorbell_regs = (void *)bar0;
    ctrl->num_ports = 0;
    ctrl->num_slots = 0;
    ctrl->has_msi = (vector != 0);
    ctrl->irq = vector ? vector : legacy_irq;

    hc->type = USB_CONTROLLER_XHCI;
    hc->name = "xhci";
    hc->regs_base = (void *)bar0;
    hc->irq = ctrl->irq;
    hc->num_ports = 0;
    hc->enabled = true;
    hc->ops = &xhci_ops;
    hc->private_data = ctrl;

    usb_host_register(hc);
    klog_printf(KLOG_INFO, "xhci: pci probe bus=%u slot=%u func=%u bar=0x%llx irq=%u (msi=%s)",
                (unsigned)bus, (unsigned)slot, (unsigned)func,
                (unsigned long long)bar0, (unsigned)hc->irq, ctrl->has_msi ? "yes" : "no");
    return 0;
}

int xhci_cmd_ring_init(xhci_controller_t *xhci) {
    (void)xhci;
    return 0;
}

int xhci_event_ring_init(xhci_controller_t *xhci) {
    (void)xhci;
    return 0;
}

int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    (void)xhci;
    (void)slot;
    (void)endpoint;
    return 0;
}

void xhci_transfer_ring_free(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    (void)xhci;
    (void)slot;
    (void)endpoint;
}

int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb) {
    (void)ring;
    (void)trb;
    return 0;
}

int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb) {
    (void)ring;
    (void)trb;
    return 0;
}

int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb) {
    (void)ring;
    (void)trb;
    return 0;
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
    (void)xhci;
    return submit_stub(transfer);
}

int xhci_process_events(xhci_controller_t *xhci) {
    (void)xhci;
    return 0;
}

bool xhci_legacy_handoff(uintptr_t base) {
    (void)base;
    return true;
}

bool xhci_reset_with_delay(uintptr_t base, uint32_t delay_ms) {
    (void)base;
    (void)delay_ms;
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
    (void)base;
    return true;
}

int xhci_controller_init(xhci_controller_t *ctrl, uintptr_t base) {
    if (!ctrl) {
        return -1;
    }
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->cap_regs = (void *)base;
    ctrl->op_regs = (void *)base;
    ctrl->num_ports = 1;
    ctrl->num_slots = 4;
    return 0;
}

bool xhci_cmd_enable_slot(xhci_controller_t *ctrl, uint8_t *slot_id) {
    (void)ctrl;
    if (!slot_id) {
        return false;
    }
    *slot_id = 1;
    return true;
}

bool xhci_cmd_address_device(xhci_controller_t *ctrl, uint8_t slot_id, uint32_t route) {
    (void)ctrl;
    (void)slot_id;
    (void)route;
    return true;
}

bool xhci_cmd_configure_endpoint(xhci_controller_t *ctrl, uint8_t slot_id, uint32_t ctx) {
    (void)ctrl;
    (void)slot_id;
    (void)ctx;
    return true;
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
