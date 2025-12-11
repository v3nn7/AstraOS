/*==========================================================
== v3nn7 ==
== AstraOS ==
   XHCI Structures (TRB, Contexts, Registers)
==========================================================*/

#pragma once
#define ASTRA_XHCI_STRUCTS_PROVIDED 1
#include <stdint.h>

//
// ========================
//  XHCI REGISTER SPACES
// ========================
//

// Capability Registers (offset 0)
typedef struct {
    uint8_t  caplength;        // 0x00
    uint8_t  reserved;
    uint16_t hciversion;       // 0x02
    uint32_t hcsparams1;       // 0x04
    uint32_t hcsparams2;       // 0x08
    uint32_t hcsparams3;       // 0x0C
    uint32_t hccparams1;       // 0x10
    uint32_t dboff;            // 0x14 - doorbell offset
    uint32_t rtsoff;           // 0x18 - runtime regs offset
} xhci_cap_regs_t;

// Operational Registers
typedef struct {
    uint32_t usbcmd;           // 0x00
    uint32_t usbsts;           // 0x04
    uint32_t pagesize;         // 0x08
    uint32_t reserved1[2];
    uint32_t dnctrl;           // 0x14
    uint64_t crcr;             // 0x18
    uint32_t reserved2[4];
    uint64_t dcbaap;           // 0x30
    uint32_t config;           // 0x38
} xhci_op_regs_t;

// Runtime Registers
typedef struct {
    uint32_t mfindex;
    uint32_t reserved[7];
} xhci_runtime_regs_t;

//
// ========================
//  XHCI DOORBELL
// ========================
//

typedef struct {
    uint32_t db_target;
} xhci_doorbell_t;

//
// ========================
//  TRB DEFINITIONS
// ========================
//

typedef struct {
    uint32_t parameter_low;
    uint32_t parameter_high;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

// TRB types
enum {
    TRB_TYPE_NORMAL            = 1,
    TRB_TYPE_SETUP_STAGE       = 2,
    TRB_TYPE_DATA_STAGE        = 3,
    TRB_TYPE_STATUS_STAGE     = 4,
    TRB_TYPE_EVENT_DATA       = 5,
    TRB_TYPE_NO_OP            = 6,

    TRB_CMD_ENABLE_SLOT        = 9,
    TRB_CMD_DISABLE_SLOT       = 10,
    TRB_CMD_ADDRESS_DEVICE     = 11,
    TRB_CMD_CONFIGURE_ENDPOINT = 12,
    TRB_CMD_EVALUATE_CONTEXT   = 13,
    TRB_CMD_RESET_ENDPOINT     = 14,
    TRB_CMD_STOP_ENDPOINT      = 15,
    TRB_CMD_SET_TR_DEQUEUE     = 16,

    TRB_EVENT_TRANSFER         = 32,
    TRB_EVENT_CMD_COMPLETION   = 33,
    TRB_EVENT_PORT_STATUS      = 34,
};

static inline uint32_t xhci_trb_type(xhci_trb_t* t) {
    return (t->control >> 10) & 0x3F;
}

#define TRB_CYCLE (1 << 0)

//
// ========================
//  DEVICE CONTEXTS
// ========================
//

// Slot Context
typedef struct {
    uint32_t route_string;
    uint32_t speed;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t tt_info;
    uint32_t max_exit_latency;
    uint32_t root_hub_port;
    uint32_t num_ports;
} xhci_slot_ctx_t;

// Endpoint Context
typedef struct {
    uint32_t ep_state;
    uint32_t reserved1;
    uint64_t tr_dequeue_ptr;
    uint16_t max_packet_size;
    uint16_t interval;
    uint32_t reserved2;
    uint32_t reserved3;
} xhci_ep_ctx_t;

// Input Control Context
typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
} xhci_input_ctrl_ctx_t;

// Input Context
typedef struct {
    xhci_input_ctrl_ctx_t ctrl_ctx;
    xhci_slot_ctx_t slot_ctx;
    xhci_ep_ctx_t   ep_ctx[31];
} xhci_input_ctx_t;

// Device Context
typedef struct {
    xhci_slot_ctx_t slot_ctx;
    xhci_ep_ctx_t   ep_ctx[31];
} xhci_device_ctx_t;

//
// ========================
//  DCBAA
// ========================
//

typedef struct {
    uint64_t device_ctx_ptrs[256];
} xhci_dcbaa_t;

//
// ========================
//  RING STRUCTURES
// ========================
//

typedef struct {
    xhci_trb_t* trbs;
    uint32_t size;
    uint32_t enqueue_idx;
    uint32_t dequeue_idx;
    uint8_t cycle_state;
} xhci_trb_ring_t;

//
// ========================
//  ERST (Event Ring Segment Table)
// ========================
//

typedef struct {
    uint64_t base;
    uint32_t size;
    uint32_t reserved;
} xhci_erst_entry_t;

typedef struct {
    xhci_erst_entry_t* entries;
    uint32_t entry_count;
} xhci_erst_t;

