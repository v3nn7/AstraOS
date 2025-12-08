#pragma once

#include "usb/usb_core.h"
#include "types.h"
#include <stddef.h>
#include <stdbool.h>

#include "usb/xhci_regs.h"
#include "usb/xhci_trb.h"
#include "usb/xhci_context.h"
#include "usb/xhci_caps.h"
#include "usb/xhci_rings.h"
#include "usb/xhci_events.h"
#include "usb/xhci_ports.h"
#include "usb/xhci_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for opaque controller type (full definition in xhci_internal.h) */
typedef struct xhci_controller xhci_controller_t;

/* XHCI Functions */
int xhci_init(usb_host_controller_t *hc);
int xhci_reset(usb_host_controller_t *hc);
int xhci_reset_port(usb_host_controller_t *hc, uint8_t port);
uint32_t xhci_enable_slot(xhci_controller_t *xhci, usb_device_t *dev, uint8_t speed);
int xhci_address_device(xhci_controller_t *xhci, uint32_t slot_id, xhci_input_context_t *input_ctx, uint64_t input_ctx_phys);

/* Transfer functions */
int xhci_transfer_control(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_transfer_interrupt(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_transfer_bulk(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_transfer_isoc(usb_host_controller_t *hc, usb_transfer_t *transfer);

/* Event processing */
int xhci_poll(usb_host_controller_t *hc);
int xhci_process_events(xhci_controller_t *xhci);
int xhci_handle_transfer_event(xhci_controller_t *xhci, xhci_transfer_event_t *event);
void xhci_handle_command_completion(xhci_controller_t *xhci, xhci_event_trb_t *event);
int xhci_handle_port_status_change(xhci_controller_t *xhci, xhci_port_status_event_t *event);

/* Cleanup */
void xhci_cleanup(usb_host_controller_t *hc);

/* XHCI PCI Detection */
int xhci_pci_probe(uint8_t bus, uint8_t slot, uint8_t func);
int xhci_pci_init(uint8_t bus, uint8_t device, uint8_t function);

/* XHCI Ring Management */
int xhci_cmd_ring_init(xhci_controller_t *xhci);
int xhci_event_ring_init(xhci_controller_t *xhci);
int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
void xhci_cmd_ring_free(xhci_controller_t *xhci);
void xhci_event_ring_free(xhci_controller_t *xhci);
void xhci_transfer_ring_free(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);

int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb);
int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb);
int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb, void *rt_regs);

/* TRB construction helpers */
void xhci_build_trb(xhci_trb_t *trb, uint64_t data_ptr, uint32_t length, uint32_t type, uint32_t flags);
void xhci_build_normal_trb(xhci_trb_t *trb, uint64_t data_buffer, uint32_t length, uint32_t td_size, bool ioc, bool isp, bool chain, bool cycle);
void xhci_build_setup_stage_trb(xhci_trb_t *trb, usb_device_request_t *setup, uint32_t transfer_type, bool cycle);
void xhci_build_data_stage_trb(xhci_trb_t *trb, uint64_t data_buffer, uint32_t length, bool direction_in, bool ioc, bool chain, bool cycle);
void xhci_build_status_stage_trb(xhci_trb_t *trb, bool direction_in, bool ioc, bool cycle);
void xhci_build_link_trb(xhci_trb_t *trb, uint64_t next_ring_addr, bool toggle_cycle);
void xhci_build_enable_slot_trb(xhci_trb_t *trb, uint8_t slot_type, bool cycle);
void xhci_build_disable_slot_trb(xhci_trb_t *trb, uint8_t slot_id, bool cycle);
void xhci_build_address_device_trb(xhci_trb_t *trb, uint64_t input_context_ptr, uint8_t slot_id, bool bsr, bool cycle);
void xhci_build_configure_endpoint_trb(xhci_trb_t *trb, uint64_t input_context_ptr, uint8_t slot_id, bool dc, bool cycle);
void xhci_build_evaluate_context_trb(xhci_trb_t *trb, uint64_t input_context_ptr, uint8_t slot_id, bool cycle);
void xhci_build_reset_endpoint_trb(xhci_trb_t *trb, uint8_t slot_id, uint8_t endpoint_id, bool tsp, bool cycle);
void xhci_build_stop_endpoint_trb(xhci_trb_t *trb, uint8_t slot_id, uint8_t endpoint_id, bool sp, bool cycle);
void xhci_build_set_tr_dequeue_trb(xhci_trb_t *trb, uint64_t dequeue_ptr, uint16_t stream_id, uint8_t slot_id, uint8_t endpoint_id, bool cycle);
void xhci_build_reset_device_trb(xhci_trb_t *trb, uint8_t slot_id, bool cycle);
void xhci_build_noop_trb(xhci_trb_t *trb, bool cycle);

/* Wait and timeout utilities */
int xhci_wait_for_register(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif