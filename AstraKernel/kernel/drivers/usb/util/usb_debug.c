/**
 * USB Debugging Tools
 * 
 * TRB dump, device tree printer, and USB trace logging.
 */

#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/xhci.h"
#include "usb/util/usb_log.h"
#include "usb/util/usb_helpers.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/**
 * Dump TRB in hex format
 */
void usb_dump_trb(const xhci_trb_t *trb) {
    if (!trb) {
        printf("TRB: NULL\n");
        return;
    }
    
    printf("TRB @ %p:\n", trb);
    printf("  Parameter: 0x%016llx\n", (unsigned long long)trb->parameter);
    printf("  Status:    0x%08x\n", trb->status);
    printf("  Control:   0x%08x\n", trb->control);
    
    uint32_t type = (trb->control >> XHCI_TRB_TYPE_SHIFT) & 0x3F;
    uint32_t length = trb->status & 0xFFFF;
    uint8_t cycle = (trb->control >> 0) & 1;
    uint8_t ioc = (trb->control >> 4) & 1;
    
    printf("  Type:      %u\n", type);
    printf("  Length:    %u\n", length);
    printf("  Cycle:    %u\n", cycle);
    printf("  IOC:       %u\n", ioc);
}

/**
 * Dump event TRB
 */
void usb_dump_event_trb(const xhci_event_trb_t *trb) {
    if (!trb) {
        printf("Event TRB: NULL\n");
        return;
    }
    
    printf("Event TRB @ %p:\n", trb);
    printf("  Data:           0x%016llx\n", (unsigned long long)trb->data);
    printf("  Status:         0x%08x\n", trb->status);
    printf("  Control:        0x%08x\n", trb->control);
    
    /* Extract fields from status and control */
    uint8_t completion_code = trb->status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
    uint32_t transfer_length = (trb->status >> XHCI_EVENT_TRB_TRANSFER_LENGTH_SHIFT) & XHCI_EVENT_TRB_TRANSFER_LENGTH_MASK;
    uint8_t slot_id = (trb->control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
    uint8_t endpoint_id = (trb->control >> XHCI_EVENT_TRB_ENDPOINT_ID_SHIFT) & XHCI_EVENT_TRB_ENDPOINT_ID_MASK;
    uint8_t trb_type = (trb->control >> XHCI_EVENT_TRB_TYPE_SHIFT) & XHCI_EVENT_TRB_TYPE_MASK;
    uint8_t cycle = (trb->control & XHCI_EVENT_TRB_CYCLE_BIT) ? 1 : 0;
    
    printf("  Completion Code: %u\n", completion_code);
    printf("  Transfer Length: %u\n", transfer_length);
    printf("  Slot ID:        %u\n", slot_id);
    printf("  Endpoint ID:    %u\n", endpoint_id);
    printf("  TRB Type:       %u\n", trb_type);
    printf("  Cycle:          %u\n", cycle);
}

/**
 * Print USB device tree
 */
void usb_print_device_tree(void) {
    extern usb_device_t *usb_device_find_by_address(uint8_t address);
    
    printf("=== USB Device Tree ===\n");
    
    uint32_t device_count = 0;
    for (uint8_t addr = 1; addr < 128; addr++) {
        usb_device_t *dev = usb_device_find_by_address(addr);
        if (!dev) continue;
        
        device_count++;
        printf("Device %u:\n", addr);
        printf("  VID:PID = %04x:%04x\n", dev->vendor_id, dev->product_id);
        printf("  Class:Subclass:Protocol = %02x:%02x:%02x\n",
               dev->device_class, dev->device_subclass, dev->device_protocol);
        printf("  Speed: %s\n", usb_speed_string(dev->speed));
        printf("  State: %d\n", dev->state);
        printf("  Endpoints: %u\n", dev->num_endpoints);
        printf("  Controller: %s\n", dev->controller ? dev->controller->name : "NULL");
        
        for (uint8_t i = 0; i < dev->num_endpoints; i++) {
            usb_endpoint_t *ep = &dev->endpoints[i];
            printf("    EP 0x%02x: type=%s max_packet=%u interval=%u\n",
                   ep->address,
                   usb_endpoint_type_string(ep->type),
                   ep->max_packet_size,
                   ep->interval);
        }
    }
    
    if (device_count == 0) {
        printf("No USB devices found\n");
    }
    
    printf("=== End Device Tree ===\n");
}

/**
 * Print XHCI controller state
 */
void usb_print_xhci_state(xhci_controller_t *xhci) {
    if (!xhci) {
        printf("XHCI: NULL\n");
        return;
    }
    
    printf("=== XHCI Controller State ===\n");
    printf("Version:     0x%04x\n", xhci->hci_version);
    printf("Slots:      %u\n", xhci->num_slots);
    printf("Ports:     %u\n", xhci->num_ports);
    printf("Interrupters: %u\n", xhci->max_interrupters);
    printf("64-bit:    %s\n", xhci->has_64bit_addressing ? "yes" : "no");
    printf("Scratchpad: %s", xhci->has_scratchpad ? "yes" : "no");
    if (xhci->has_scratchpad) {
        printf(" (%u buffers)", xhci->scratchpad_size);
    }
    printf("\n");
    printf("MSI:        %s\n", xhci->has_msi ? "yes" : "no");
    printf("IRQ:        %u\n", xhci->irq);
    printf("Command Ring: %p (size=%u)\n", xhci->cmd_ring.trbs, xhci->cmd_ring.size);
    printf("Event Ring:   %p (size=%u)\n", xhci->event_ring.trbs, xhci->event_ring.size);
    printf("DCBAAP:       %p\n", xhci->dcbaap);
    
    printf("Allocated Slots: ");
    bool any_slots = false;
    for (uint32_t i = 0; i < 32; i++) {
        if (xhci->slot_allocated[i]) {
            printf("%u ", i);
            any_slots = true;
        }
    }
    if (!any_slots) {
        printf("none");
    }
    printf("\n");
    
    printf("Port Status:\n");
    for (uint32_t i = 0; i < xhci->num_ports && i < 32; i++) {
        if (xhci->port_status[i] != 0) {
            printf("  Port %u: 0x%08x\n", i, xhci->port_status[i]);
        }
    }
    
    printf("=== End XHCI State ===\n");
}

/**
 * Print USB transfer information
 */
void usb_print_transfer(const usb_transfer_t *transfer) {
    if (!transfer) {
        printf("Transfer: NULL\n");
        return;
    }
    
    printf("=== USB Transfer ===\n");
    printf("Device:     %p\n", transfer->device);
    printf("Endpoint:   %p (addr=0x%02x)\n", transfer->endpoint,
           transfer->endpoint ? transfer->endpoint->address : 0);
    printf("Buffer:     %p\n", transfer->buffer);
    printf("Length:     %zu\n", transfer->length);
    printf("Actual:     %zu\n", transfer->actual_length);
    printf("Status:     %d\n", transfer->status);
    printf("Timeout:    %u ms\n", transfer->timeout_ms);
    printf("Control:    %s\n", transfer->is_control ? "yes" : "no");
    
    if (transfer->is_control) {
        printf("Setup:     ");
        for (int i = 0; i < 8; i++) {
            printf("%02x ", transfer->setup[i]);
        }
        printf("\n");
    }
    
    printf("=== End Transfer ===\n");
}

