#pragma once

#include "usb/usb_core.h"
#include "usb/xhci_regs.h"  /* Register structures */
#include "types.h"
#include "pmm.h"
#include <stddef.h>  /* For offsetof */

/**
 * XHCI (eXtensible Host Controller Interface) Driver
 * 
 * Full TRB-based implementation for USB 3.0+ controllers.
 * Supports MSI/MSI-X interrupts and legacy IRQ fallback.
 */

/* XHCI Capability Registers - Offset Macros (for compatibility) */
#define XHCI_CAPLENGTH           0x00
#define XHCI_HCIVERSION          0x02
#define XHCI_HCSPARAMS1          0x04
#define XHCI_HCSPARAMS2          0x08
#define XHCI_HCSPARAMS3          0x0C
#define XHCI_HCCPARAMS           0x10
#define XHCI_DBOFF               0x14
#define XHCI_RTSOFF              0x18

/* XHCI Operational Registers - Offset Macros (for compatibility) */
#define XHCI_USBCMD              0x00
#define XHCI_USBSTS              0x04
#define XHCI_PAGESIZE            0x08
#define XHCI_DNCTRL              0x14
#define XHCI_CRCR                0x18
#define XHCI_DCBAAP              0x30
#define XHCI_CONFIG              0x38

/* XHCI Runtime Registers */
/* Runtime registers layout:
 * - mfindex at 0x0
 * - 7 reserved uint32_t at 0x4-0x20
 * - Interrupter 0 starts at 0x20
 * - Each interrupter is 0x20 bytes
 * - Within interrupter: IMAN(0x00), IMOD(0x04), ERSTSZ(0x08), ERSTBA(0x10), ERDP(0x18)
 */
#define XHCI_IMAN(n)             (0x20 + ((n) * 0x20) + 0x00)
#define XHCI_IMOD(n)             (0x20 + ((n) * 0x20) + 0x04)
#define XHCI_ERSTSZ(n)           (0x20 + ((n) * 0x20) + 0x08)
#define XHCI_ERSTBA(n)           (0x20 + ((n) * 0x20) + 0x10)
#define XHCI_ERDP(n)             (0x20 + ((n) * 0x20) + 0x18)

/* XHCI Runtime Register Bits */
#define XHCI_ERSTSZ_ERST_SZ_MASK 0xFFFF
#define XHCI_ERDP_EHB           (1 << 3)
#define XHCI_CRCR_RCS           (1ULL << 0)  /* Ring Cycle State */
#define XHCI_CRCR_CSS           (1ULL << 1)  /* Command Ring Cycle State - CRITICAL! */
#define XHCI_CRCR_CA            (1ULL << 2)  /* Command Abort */
#define XHCI_CRCR_CRR           (1ULL << 3)  /* Command Ring Running */
#define XHCI_CRCR_CS            XHCI_CRCR_CSS  /* Alias for CSS */

/* XHCI Port Registers */
#define XHCI_PORTSC(n)           (0x400 + ((n) * 0x10))
#define XHCI_PORTPMSC(n)         (0x400 + ((n) * 0x10) + 0x04)
#define XHCI_PORTLI(n)           (0x400 + ((n) * 0x10) + 0x08)

/* XHCI Command Bits */
#define XHCI_CMD_RUN             (1 << 0)
#define XHCI_CMD_HCRST           (1 << 1)
#define XHCI_CMD_INTE            (1 << 2)
#define XHCI_CMD_HSEE            (1 << 3)

/* XHCI Status Bits */
#define XHCI_STS_HCH             (1 << 0)
#define XHCI_STS_HSE             (1 << 2)
#define XHCI_STS_EINT            (1 << 3)
#define XHCI_STS_PCD             (1 << 4)
#define XHCI_STS_CNR             (1 << 11)

/* XHCI Port Status */
#define XHCI_PORTSC_CCS          (1 << 0)
#define XHCI_PORTSC_PED          (1 << 1)
#define XHCI_PORTSC_OCA          (1 << 3)
#define XHCI_PORTSC_PR           (1 << 4)
#define XHCI_PORTSC_PLS_SHIFT    5
#define XHCI_PORTSC_PP           (1 << 9)
#define XHCI_PORTSC_SPEED_SHIFT  10
/* Mask of non-RW bits to clear when preserving writable fields during port ops */
#define XHCI_PORTSC_RW_MASK      (~((uint32_t)(XHCI_PORTSC_CCS | XHCI_PORTSC_PED | (0xF << XHCI_PORTSC_PLS_SHIFT))))

/* USB Speeds (for Slot Context) */
#define XHCI_SPEED_FULL    1
#define XHCI_SPEED_LOW     2
#define XHCI_SPEED_HIGH    3
#define XHCI_SPEED_SUPER   4

/* TRB Types */
#define XHCI_TRB_TYPE_NORMAL     1
#define XHCI_TRB_TYPE_SETUP_STAGE 2
#define XHCI_TRB_TYPE_DATA_STAGE 3
#define XHCI_TRB_TYPE_STATUS_STAGE 4
#define XHCI_TRB_TYPE_ISOCH      5
#define XHCI_TRB_TYPE_LINK       6
#define XHCI_TRB_TYPE_EVENT_DATA 7
#define XHCI_TRB_TYPE_NOOP       8
#define XHCI_TRB_TYPE_ENABLE_SLOT 9
#define XHCI_TRB_TYPE_DISABLE_SLOT 10
#define XHCI_TRB_TYPE_ADDRESS_DEVICE 11
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT 12
#define XHCI_TRB_TYPE_EVALUATE_CONTEXT 13
#define XHCI_TRB_TYPE_RESET_ENDPOINT 14
#define XHCI_TRB_TYPE_STOP_ENDPOINT 15
#define XHCI_TRB_TYPE_SET_TR_DEQUEUE 16
#define XHCI_TRB_TYPE_RESET_DEVICE 17
#define XHCI_TRB_TYPE_FORCE_EVENT 18
#define XHCI_TRB_TYPE_NEGOTIATE_BANDWIDTH 19
#define XHCI_TRB_TYPE_SET_LATENCY_TOLERANCE 20
#define XHCI_TRB_TYPE_GET_PORT_BANDWIDTH 21
#define XHCI_TRB_TYPE_FORCE_HEADER 22
#define XHCI_TRB_TYPE_NOOP_CMD   23

/* TRB Completion Codes */
#define XHCI_TRB_CC_SUCCESS      1
#define XHCI_TRB_CC_DATA_BUFFER_ERROR 2
#define XHCI_TRB_CC_BABBLE_DETECTED 3
#define XHCI_TRB_CC_USB_TRANSACTION_ERROR 4
#define XHCI_TRB_CC_TRB_ERROR    5
#define XHCI_TRB_CC_STALL_ERROR  6
#define XHCI_TRB_CC_RESOURCE_ERROR 7
#define XHCI_TRB_CC_BANDWIDTH_ERROR 8
#define XHCI_TRB_CC_NO_SLOTS_AVAILABLE 9
#define XHCI_TRB_CC_INVALID_STREAM_TYPE 10
#define XHCI_TRB_CC_SLOT_NOT_ENABLED 11
#define XHCI_TRB_CC_ENDPOINT_NOT_ENABLED 12
#define XHCI_TRB_CC_SHORT_PACKET 13
#define XHCI_TRB_CC_RING_UNDERRUN 14
#define XHCI_TRB_CC_RING_OVERRUN 15
#define XHCI_TRB_CC_VF_EVENT_RING_FULL 16
#define XHCI_TRB_CC_PARAMETER_ERROR 17
#define XHCI_TRB_CC_BANDWIDTH_OVERRUN 18
#define XHCI_TRB_CC_CONTEXT_STATE_ERROR 19
#define XHCI_TRB_CC_NO_PING_RESPONSE 20
#define XHCI_TRB_CC_EVENT_RING_FULL 21
#define XHCI_TRB_CC_INCOMPATIBLE_DEVICE 22
#define XHCI_TRB_CC_MISSED_SERVICE_ERROR 23
#define XHCI_TRB_CC_COMMAND_RING_STOPPED 24
#define XHCI_TRB_CC_COMMAND_ABORTED 25
#define XHCI_TRB_CC_STOPPED 26
#define XHCI_TRB_CC_STOPPED_LENGTH_INVALID 27
#define XHCI_TRB_CC_STOPPED_SHORT_PACKET 28
#define XHCI_TRB_CC_MAX_EXIT_LATENCY_TOO_LARGE 29
#define XHCI_TRB_CC_ISOCH_BUFFER_OVERRUN 31
#define XHCI_TRB_CC_EVENT_LOST_ERROR 32
#define XHCI_TRB_CC_UNDEFINED_ERROR 33
#define XHCI_TRB_CC_INVALID_STREAM_ID_ERROR 34
#define XHCI_TRB_CC_SECONDARY_BANDWIDTH_ERROR 35
#define XHCI_TRB_CC_SPLIT_TRANSACTION_ERROR 36

/* TRB Structure (64 bytes for 64-bit systems) */
typedef struct PACKED {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

/* TRB Control Field Bits */
#define XHCI_TRB_CYCLE           (1 << 0)
#define XHCI_TRB_TC              (1 << 1)
#define XHCI_TRB_ISP             (1 << 2)
#define XHCI_TRB_IOC             (1 << 4)
#define XHCI_TRB_IDT             (1 << 5)
#define XHCI_TRB_TLBPC_SHIFT     16
#define XHCI_TRB_TYPE_SHIFT      10
#define XHCI_TRB_TYPE_MASK       0x3F  /* 6 bits for TRB type */
#define XHCI_TRB_TRT_SHIFT       16

/* Event TRB */
typedef struct PACKED {
    uint64_t data; /* Pointer to TRB that completed */
    uint32_t status; /* Status field: completion_code[7:0], transfer_length[23:8], etc. */
    uint32_t control; /* Control field: slot_id, endpoint_id, trb_type, cycle */
} xhci_event_trb_t;

/* Event TRB Status Field Bits */
#define XHCI_EVENT_TRB_COMPLETION_CODE_MASK 0xFF
#define XHCI_EVENT_TRB_TRANSFER_LENGTH_SHIFT 16
#define XHCI_EVENT_TRB_TRANSFER_LENGTH_MASK 0x1FFFF

/* Event TRB Control Field Bits */
#define XHCI_EVENT_TRB_SLOT_ID_SHIFT 24
#define XHCI_EVENT_TRB_SLOT_ID_MASK 0xFF
#define XHCI_EVENT_TRB_ENDPOINT_ID_SHIFT 16
#define XHCI_EVENT_TRB_ENDPOINT_ID_MASK 0x1F
#define XHCI_EVENT_TRB_TYPE_SHIFT 10
#define XHCI_EVENT_TRB_TYPE_MASK 0x3F
#define XHCI_EVENT_TRB_CYCLE_BIT (1 << 0)

/* Transfer Ring */
typedef struct {
    xhci_trb_t *trbs;
    uint32_t size; /* Number of TRBs */
    uint32_t dequeue;
    uint32_t enqueue;
    bool cycle_state;
    phys_addr_t phys_addr;
} xhci_transfer_ring_t;

/* Command Ring */
typedef struct {
    xhci_trb_t *trbs;
    uint32_t size;
    uint32_t dequeue;
    uint32_t enqueue;
    bool cycle_state;
    phys_addr_t phys_addr;
} xhci_command_ring_t;

/* Event Ring */
typedef struct {
    xhci_event_trb_t *trbs;
    uint32_t size;
    uint32_t dequeue;
    uint32_t enqueue;
    bool cycle_state;
    phys_addr_t phys_addr;
    phys_addr_t segment_table_phys;
    void *segment_table;
} xhci_event_ring_t;

/* Device Context */
typedef struct PACKED {
    uint64_t in_context;
    uint64_t out_context;
} xhci_device_context_t;

/* Input Context */
typedef struct PACKED {
    uint32_t drop_context_flags;
    uint32_t add_context_flags;
    uint32_t reserved[6];
    /* Slot Context */
    uint32_t slot_context[8];
    /* Endpoint Contexts (up to 31) */
    uint32_t endpoint_context[31][8];
} xhci_input_context_t;

/* Slot Context */
typedef struct PACKED {
    uint32_t route_string:20;
    uint32_t speed:4;
    uint32_t reserved:1;
    uint32_t mtt:1;
    uint32_t hub:1;
    uint32_t context_entries:5;
    uint32_t max_exit_latency:16;
    uint32_t root_hub_port_number:8;
    uint32_t num_ports:8;
    uint32_t parent_hub_slot_id:8;
    uint32_t parent_port_number:8;
    uint32_t ttt:2;
    uint32_t reserved2:4;
    uint32_t interrupter_target:10;
    uint32_t usb_device_address:8;
    uint32_t reserved3:19;
    uint32_t slot_state:5;
} xhci_slot_context_t;

/* Endpoint Context */
typedef struct PACKED {
    uint32_t ep_state:3;
    uint32_t reserved:5;
    uint32_t mult:2;
    uint32_t max_pstreams:5;
    uint32_t lsa:1;
    uint32_t interval:8;
    uint32_t max_esit_payload_hi:8;
    uint32_t reserved2:1;
    uint32_t error_count:2;
    uint32_t ep_type:3;
    uint32_t reserved3:1;
    uint32_t max_packet_size:16;
    uint32_t max_burst_size:8;
    uint32_t max_esit_payload_lo:8;
    uint32_t reserved4:4;
    uint32_t average_trb_length:16;
    uint64_t dequeue_pointer;
    uint32_t reserved5:16;
    uint32_t interrupter_target:10;
    uint32_t reserved6:6;
} xhci_endpoint_context_t;

/* XHCI Controller Private Data */
typedef struct {
    xhci_cap_regs_t *cap_regs;          // Capability Registers (MMIO mapped)
    xhci_op_regs_t *op_regs;            // Operational Registers (MMIO mapped)
    xhci_rt_regs_t *rt_regs;            // Runtime Registers (MMIO mapped)
    xhci_doorbell_regs_t *doorbell_regs; // Doorbell Registers (MMIO mapped)
    uint32_t cap_length;
    uint32_t hci_version;
    uint32_t num_slots;
    uint32_t num_ports;
    uint32_t max_interrupters;
    bool has_64bit_addressing;
    bool has_scratchpad;
    uint32_t scratchpad_size;
    void **scratchpad_buffers;
    bool has_msi;
    uint32_t irq;
    xhci_command_ring_t cmd_ring;
    xhci_event_ring_t event_ring;
    xhci_transfer_ring_t *transfer_rings[32][32]; /* [slot][endpoint] */
    xhci_device_context_t *device_contexts[32];
    uint64_t *dcbaap; /* Device Context Base Address Array */
    uint8_t slot_allocated[32];
    uint32_t port_status[32];
    struct usb_transfer *active_control_transfers[32]; /* Active control transfers per slot */
} xhci_controller_t;

/* XHCI Functions */
int xhci_init(usb_host_controller_t *hc);
int xhci_reset(usb_host_controller_t *hc);
int xhci_reset_port(usb_host_controller_t *hc, uint8_t port);
int xhci_transfer_control(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_transfer_interrupt(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_transfer_bulk(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_transfer_isoc(usb_host_controller_t *hc, usb_transfer_t *transfer);
int xhci_poll(usb_host_controller_t *hc);
void xhci_cleanup(usb_host_controller_t *hc);

/* XHCI PCI Detection */
int xhci_pci_probe(uint8_t bus, uint8_t slot, uint8_t func);

/* XHCI Ring Management */
int xhci_cmd_ring_init(xhci_controller_t *xhci);
int xhci_event_ring_init(xhci_controller_t *xhci);
int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
void xhci_transfer_ring_free(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb);
int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb);
int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb, void *rt_regs);
void xhci_build_trb(xhci_trb_t *trb, uint64_t data_ptr, uint32_t length, uint32_t type, uint32_t flags);
void xhci_build_link_trb(xhci_trb_t *trb, uint64_t next_ring_addr, uint8_t toggle_cycle);

/* XHCI Transfer Functions */
int xhci_submit_control_transfer(xhci_controller_t *xhci, usb_transfer_t *transfer);
int xhci_process_events(xhci_controller_t *xhci);

/* XHCI Context Functions */
xhci_input_context_t *xhci_input_context_alloc(void);
void xhci_input_context_free(xhci_input_context_t *ctx);
void xhci_input_context_set_slot(xhci_input_context_t *ctx, uint8_t slot_id, uint8_t root_port,
                                  uint8_t speed, uint8_t address, bool hub, uint8_t parent_slot,
                                  uint8_t parent_port);
void xhci_input_context_set_ep0(xhci_input_context_t *ctx, xhci_transfer_ring_t *transfer_ring,
                                 uint16_t max_packet_size);
phys_addr_t xhci_input_context_get_phys(xhci_input_context_t *ctx);

/* XHCI Device Functions */
uint32_t xhci_enable_slot(xhci_controller_t *xhci);
int xhci_address_device(xhci_controller_t *xhci, uint32_t slot_id, xhci_input_context_t *input_ctx,
                        uint64_t input_ctx_phys);
void xhci_handle_command_completion(xhci_controller_t *xhci, xhci_event_trb_t *event);

/* XHCI Doorbell Functions */
void xhci_ring_cmd_doorbell(xhci_controller_t *xhci);
void xhci_ring_doorbell(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, uint16_t stream_id);

/* XHCI Interrupt Functions */
void xhci_register_irq_handler(usb_host_controller_t *hc, uint8_t vector);

/* XHCI Debug Functions */
const char *xhci_trb_type_str(uint8_t type);
void xhci_dump_trb(const char *label, xhci_trb_t *trb);
void xhci_dump_trb_level(int log_level, const char *label, xhci_trb_t *trb);
void xhci_dump_event_trb(const char *label, xhci_event_trb_t *event);

/* XHCI Trace Functions */
void xhci_trace_cmd_ring(xhci_controller_t *xhci);
void xhci_trace_event_ring(xhci_controller_t *xhci);
void xhci_trace_transfer_ring(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
void xhci_trace_controller(xhci_controller_t *xhci);

