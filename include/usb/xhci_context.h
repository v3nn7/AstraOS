#pragma once

#include "types.h"
#include <stdbool.h>
#include <stdbool.h>
#include <stdint.h>

struct xhci_transfer_ring;

/* Context entry flags */
#define XHCI_CTX_ENTRY_SLOT      (1u << 0)
#define XHCI_CTX_ENTRY_EP(n)     (1u << ((n) + 1))

/* Slot Context - Complete */
typedef struct PACKED {
    /* DWord 0 */
    uint32_t route_string:20;
    uint32_t speed:4;
    uint32_t reserved1:1;
    uint32_t mtt:1;
    uint32_t hub:1;
    uint32_t context_entries:5;
    
    /* DWord 1 */
    uint32_t max_exit_latency:16;
    uint32_t root_hub_port_number:8;
    uint32_t num_ports:8;
    
    /* DWord 2 */
    uint32_t parent_hub_slot_id:8;
    uint32_t parent_port_number:8;
    uint32_t ttt:2;
    uint32_t reserved2:4;
    uint32_t interrupter_target:10;
    
    /* DWord 3 */
    uint32_t usb_device_address:8;
    uint32_t reserved3:19;
    uint32_t slot_state:5;
    
    /* DWords 4-7 - Reserved */
    uint32_t reserved4[4];
} xhci_slot_context_t;

/* Slot States */
#define XHCI_SLOT_STATE_DISABLED_ENABLED 0
#define XHCI_SLOT_STATE_DEFAULT     1
#define XHCI_SLOT_STATE_ADDRESSED   2
#define XHCI_SLOT_STATE_CONFIGURED  3
#define XHCI_SLOT_STATE_ENABLED     XHCI_SLOT_STATE_DISABLED_ENABLED

/* Endpoint Context - Complete */
typedef struct PACKED {
    /* DWord 0 */
    uint32_t ep_state:3;
    uint32_t reserved1:5;
    uint32_t mult:2;
    uint32_t max_pstreams:5;
    uint32_t lsa:1;
    uint32_t interval:8;
    uint32_t max_esit_payload_hi:8;
    
    /* DWord 1 */
    uint32_t reserved2:1;
    uint32_t error_count:2;
    uint32_t ep_type:3;
    uint32_t reserved3:1;
    uint32_t hid:1;
    uint32_t max_burst_size:8;
    uint32_t max_packet_size:16;
    
    /* DWord 2 */
    uint32_t dcs:1;
    uint32_t reserved4:3;
    uint32_t tr_dequeue_pointer_lo:28;
    
    /* DWord 3 */
    uint32_t tr_dequeue_pointer_hi;
    uint32_t interrupter_target;
    
    /* DWord 4 */
    uint32_t average_trb_length:16;
    uint32_t max_esit_payload_lo:16;
    
    /* DWords 5-7 - Reserved */
    uint32_t reserved5[3];
} xhci_endpoint_context_t;

/* Endpoint States */
#define XHCI_EP_STATE_DISABLED      0
#define XHCI_EP_STATE_RUNNING       1
#define XHCI_EP_STATE_HALTED        2
#define XHCI_EP_STATE_STOPPED       3
#define XHCI_EP_STATE_ERROR         4

/* Endpoint Types */
#define XHCI_EP_TYPE_INVALID        0
#define XHCI_EP_TYPE_ISOCH_OUT      1
#define XHCI_EP_TYPE_BULK_OUT       2
#define XHCI_EP_TYPE_INTERRUPT_OUT  3
#define XHCI_EP_TYPE_CONTROL        4
#define XHCI_EP_TYPE_ISOCH_IN       5
#define XHCI_EP_TYPE_BULK_IN        6
#define XHCI_EP_TYPE_INTERRUPT_IN   7

/* Stream Context */
typedef struct PACKED {
    /* DWord 0 */
    uint32_t dcs:1;
    uint32_t sct:3;
    uint32_t tr_dequeue_pointer_lo:28;
    
    /* DWord 1 */
    uint32_t tr_dequeue_pointer_hi;
    
    /* DWords 2-3 - Reserved */
    uint32_t reserved[2];
} xhci_stream_context_t;

/* Input Control Context */
typedef struct PACKED {
    uint32_t drop_context_flags;
    uint32_t add_context_flags;
    uint32_t reserved[5];
    uint32_t configuration_value:8;
    uint32_t interface_number:8;
    uint32_t alternate_setting:8;
    uint32_t reserved2:8;
} xhci_input_control_context_t;

/* Input Context */
typedef struct PACKED {
    xhci_input_control_context_t control;
    xhci_slot_context_t slot;
    xhci_endpoint_context_t endpoints[31];
} xhci_input_context_t;

/* Device Context */
typedef struct PACKED {
    xhci_slot_context_t slot;
    xhci_endpoint_context_t endpoints[31];
} xhci_device_context_t;

/* Context helpers (implemented in xhci_context.c) */
xhci_input_context_t *xhci_input_context_alloc(void);
void xhci_input_context_free(xhci_input_context_t *ctx);
void xhci_input_context_set_slot(xhci_input_context_t *ctx, uint8_t slot_id, uint8_t root_port,
                                 uint8_t speed, uint8_t address, bool hub, uint8_t parent_slot,
                                 uint8_t parent_port);
void xhci_input_context_set_ep0(xhci_input_context_t *ctx, struct xhci_transfer_ring *transfer_ring,
                                uint16_t max_packet_size);
phys_addr_t xhci_input_context_get_phys(xhci_input_context_t *ctx);
#pragma once

#include "usb/xhci.h"

