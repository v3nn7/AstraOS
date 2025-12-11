#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_controller.h"
#include "xhci_regs.h"
#include "xhci_trb.h"
#include "xhci_context.h"
#include "msi.h"
#include "usb_transfer.h"
#include "usb_request.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xhci_trb_ring {
    xhci_trb_t *trbs;
    uint32_t count;
    uint32_t enqueue;
    uint32_t dequeue;
    uint8_t cycle;
} xhci_trb_ring_t;

typedef struct xhci_erst_entry {
    uint64_t segment_base;
    uint16_t segment_size;
    uint16_t reserved0;
    uint32_t reserved1;
} xhci_erst_entry_t;

typedef struct xhci_event_ring {
    xhci_trb_ring_t ring;
    xhci_erst_entry_t *erst;
    uint32_t erst_size;
} xhci_event_ring_t;

/* xHCI controller-specific state wrapper around generic USB controller. */
typedef struct xhci_controller {
    usb_controller_t base;
    volatile xhci_capability_regs_t *cap_regs;
    volatile xhci_operational_regs_t *op_regs;
    uint64_t doorbell_array_phys;
    uint64_t runtime_regs_phys;
    uintptr_t mmio_base;
    xhci_trb_ring_t cmd_ring;
    xhci_event_ring_t evt_ring;
    msi_config_t msi_cfg;
} xhci_controller_t;

/* Firmware/platform helpers. */
bool xhci_legacy_handoff(uintptr_t mmio_base);
bool xhci_reset_with_delay(uintptr_t mmio_base, uint8_t cap_length);
bool xhci_require_alignment(uint64_t ptr, uint64_t align);
bool xhci_configure_msi(msi_config_t* cfg_out);
bool xhci_check_lowmem(uint64_t phys_addr);
bool xhci_route_ports(uintptr_t mmio_base);

/* Ring helpers. */
bool xhci_ring_init(xhci_trb_ring_t* ring, uint32_t trb_count);
xhci_trb_t* xhci_ring_enqueue(xhci_trb_ring_t* ring, uint32_t trb_type);
bool xhci_ring_pop(xhci_trb_ring_t* ring, xhci_trb_t* out);

/* Event ring helpers. */
bool xhci_event_ring_init(xhci_event_ring_t* evt, uint32_t trb_count);
bool xhci_event_push(xhci_event_ring_t* evt, const xhci_trb_t* trb);
bool xhci_event_pop(xhci_event_ring_t* evt, xhci_trb_t* out);

/* Controller lifecycle. */
bool xhci_controller_init(xhci_controller_t* ctrl, uintptr_t mmio_base);
uint32_t xhci_poll_events(xhci_controller_t* ctrl);
bool xhci_reap_and_arm(xhci_controller_t* ctrl);

/* Command helpers (simulated completions). */
bool xhci_cmd_enable_slot(xhci_controller_t* ctrl, uint8_t* slot_out);
bool xhci_cmd_address_device(xhci_controller_t* ctrl, uint8_t slot_id, uint64_t input_ctx_phys);
bool xhci_cmd_configure_endpoint(xhci_controller_t* ctrl, uint8_t slot_id, uint64_t input_ctx_phys);
bool xhci_build_control_transfer(xhci_trb_ring_t* ring, const usb_transfer_t* xfer,
                                 const usb_setup_packet_t* setup, uint64_t buffer_phys);

#ifdef __cplusplus
}
#endif

#endif /* XHCI_H */
