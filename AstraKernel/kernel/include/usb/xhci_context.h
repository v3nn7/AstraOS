#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note: xhci_transfer_ring_t is defined in xhci.h */
/* We use forward declaration to avoid circular dependency */
struct xhci_transfer_ring;

/**
 * XHCI Context Structures
 * 
 * Complete context definitions per XHCI 1.2 specification.
 * All structures are PACKED and must be 32-byte or 64-byte aligned.
 */

/* Context Alignment Requirements */
#define XHCI_CONTEXT_ALIGNMENT       64  /* All contexts must be 64-byte aligned */
#define XHCI_SLOT_CTX_SIZE          32  /* Slot Context = 8 words = 32 bytes */
#define XHCI_EP_CTX_SIZE            32  /* Endpoint Context = 8 words = 32 bytes */
#define XHCI_INPUT_CTX_SIZE        256  /* Input Context = 64 bytes (control) + 32*31 (endpoints) = 1056 bytes, but we use 256 for simplicity */
#define XHCI_DEVICE_CTX_SIZE       256  /* Device Context = Slot + Endpoints, minimum 64+32=96, we use 256 */

/* Context Entry Masks */
#define XHCI_CTX_ENTRY_SLOT         (1 << 0)
#define XHCI_CTX_ENTRY_EP(n)        (1 << ((n) + 1))

/* Slot Context Field Offsets and Masks */
#define XHCI_SLOT_CTX_ROUTE_SHIFT       0
#define XHCI_SLOT_CTX_ROUTE_MASK        0xFFFFF  /* 20 bits */
#define XHCI_SLOT_CTX_SPEED_SHIFT       20
#define XHCI_SLOT_CTX_SPEED_MASK        0xF      /* 4 bits */
#define XHCI_SLOT_CTX_MTT_SHIFT         25
#define XHCI_SLOT_CTX_HUB_SHIFT         26
#define XHCI_SLOT_CTX_ENTRIES_SHIFT     27
#define XHCI_SLOT_CTX_ENTRIES_MASK      0x1F     /* 5 bits */
#define XHCI_SLOT_CTX_PORT_SHIFT        16
#define XHCI_SLOT_CTX_PORT_MASK         0xFF
#define XHCI_SLOT_CTX_PARENT_SLOT_SHIFT 8
#define XHCI_SLOT_CTX_PARENT_PORT_SHIFT 0
#define XHCI_SLOT_CTX_TTT_SHIFT         10
#define XHCI_SLOT_CTX_INTERRUPTER_SHIFT 22
#define XHCI_SLOT_CTX_INTERRUPTER_MASK  0x3FF    /* 10 bits */
#define XHCI_SLOT_CTX_ADDRESS_SHIFT     0
#define XHCI_SLOT_CTX_ADDRESS_MASK      0xFF
#define XHCI_SLOT_CTX_STATE_SHIFT       27
#define XHCI_SLOT_CTX_STATE_MASK        0x1F     /* 5 bits */

/* Endpoint Context Field Offsets and Masks */
#define XHCI_EP_CTX_STATE_SHIFT         0
#define XHCI_EP_CTX_STATE_MASK          0x7      /* 3 bits */
#define XHCI_EP_CTX_MULT_SHIFT          8
#define XHCI_EP_CTX_MULT_MASK           0x3      /* 2 bits */
#define XHCI_EP_CTX_MAX_PSTREAMS_SHIFT  10
#define XHCI_EP_CTX_MAX_PSTREAMS_MASK   0x1F     /* 5 bits */
#define XHCI_EP_CTX_LSA_SHIFT           15
#define XHCI_EP_CTX_INTERVAL_SHIFT      16
#define XHCI_EP_CTX_INTERVAL_MASK       0xFF
#define XHCI_EP_CTX_MAX_ESIT_HI_SHIFT   24
#define XHCI_EP_CTX_ERROR_COUNT_SHIFT   1
#define XHCI_EP_CTX_ERROR_COUNT_MASK   0x3      /* 2 bits */
#define XHCI_EP_CTX_EP_TYPE_SHIFT      3
#define XHCI_EP_CTX_EP_TYPE_MASK       0x7      /* 3 bits */
#define XHCI_EP_CTX_MAX_PACKET_SHIFT   16
#define XHCI_EP_CTX_MAX_PACKET_MASK    0xFFFF
#define XHCI_EP_CTX_MAX_BURST_SHIFT    8
#define XHCI_EP_CTX_MAX_BURST_MASK     0xFF
#define XHCI_EP_CTX_MAX_ESIT_LO_SHIFT  0
#define XHCI_EP_CTX_AVG_TRB_LEN_SHIFT  16
#define XHCI_EP_CTX_DEQUEUE_SHIFT      4
#define XHCI_EP_CTX_DCS_BIT            0x1      /* Dequeue Cycle State bit */
#define XHCI_EP_CTX_INTERRUPTER_SHIFT  22
#define XHCI_EP_CTX_INTERRUPTER_MASK   0x3FF    /* 10 bits */

/* Endpoint States */
#define XHCI_EP_STATE_DISABLED      0
#define XHCI_EP_STATE_RUNNING       1
#define XHCI_EP_STATE_HALTED        2
#define XHCI_EP_STATE_STOPPED        3
#define XHCI_EP_STATE_ERROR         4

/* Endpoint Types */
#define XHCI_EP_TYPE_ISOCH_OUT      1
#define XHCI_EP_TYPE_BULK_OUT      2
#define XHCI_EP_TYPE_INTERRUPT_OUT  3
#define XHCI_EP_TYPE_CONTROL        4  /* Control Endpoint (bidirectional) */
#define XHCI_EP_TYPE_ISOCH_IN       5
#define XHCI_EP_TYPE_BULK_IN        6
#define XHCI_EP_TYPE_INTERRUPT_IN    7

/* Slot States */
#define XHCI_SLOT_STATE_DISABLED    0
#define XHCI_SLOT_STATE_ENABLED     1
#define XHCI_SLOT_STATE_DEFAULT     2
#define XHCI_SLOT_STATE_ADDRESSED   3
#define XHCI_SLOT_STATE_CONFIGURED  4

/* USB Speeds (for Slot Context) */
#define XHCI_SPEED_FULL             1
#define XHCI_SPEED_LOW              2
#define XHCI_SPEED_HIGH             3
#define XHCI_SPEED_SUPER            4  /* USB 3.0 SuperSpeed */
#define XHCI_SPEED_SUPER_PLUS       5  /* USB 3.1 SuperSpeedPlus */

/**
 * Input Control Context
 * First 32 bytes of Input Context
 * Word 0: Drop Context Flags
 * Word 1: Add Context Flags
 * Words 2-7: Reserved
 */
typedef struct PACKED {
    uint32_t drop_context_flags;  /* Bit 0 = Slot, Bit n+1 = Endpoint n */
    uint32_t add_context_flags;   /* Bit 0 = Slot, Bit n+1 = Endpoint n */
    uint32_t reserved[6];         /* Reserved words */
} xhci_input_control_context_t;

/**
 * Slot Context
 * 8 words (32 bytes) per XHCI 1.2 spec
 * Word 0: Route String [19:0], Speed [23:20], MTT [25], Hub [26], Context Entries [31:27]
 * Word 1: Max Exit Latency [15:0]
 * Word 2: Root Hub Port Number [23:16], Number of Ports [31:24]
 * Word 3: Parent Hub Slot ID [7:0], Parent Port Number [15:8]
 * Word 4: TTT [9:8], Reserved [21:10], Interrupter Target [31:22]
 * Word 5: USB Device Address [7:0], Reserved [26:8], Slot State [31:27]
 * Words 6-7: Reserved
 */
typedef struct PACKED {
    /* Word 0 */
    uint32_t route_string:20;        /* Route String for USB 3.0 hubs */
    uint32_t speed:4;                /* USB Speed: 1=Full, 2=Low, 3=High, 4=Super, 5=SuperPlus */
    uint32_t reserved0:1;
    uint32_t mtt:1;                  /* Multi-TT (for USB 2.0 hubs) */
    uint32_t hub:1;                  /* 1 if hub device */
    uint32_t context_entries:5;      /* Number of endpoint contexts (1-31) */
    
    /* Word 1 */
    uint32_t max_exit_latency:16;    /* Max Exit Latency (for hubs) */
    uint32_t reserved1:16;
    
    /* Word 2 */
    uint32_t reserved2:16;
    uint32_t root_hub_port_number:8; /* Root Hub Port Number */
    uint32_t num_ports:8;            /* Number of ports (for hubs) */
    
    /* Word 3 */
    uint32_t parent_port_number:8;   /* Parent Port Number */
    uint32_t parent_hub_slot_id:8;   /* Parent Hub Slot ID */
    uint32_t reserved3:16;
    
    /* Word 4 */
    uint32_t reserved4:10;
    uint32_t ttt:2;                  /* TT Think Time (for USB 2.0 hubs) */
    uint32_t reserved5:10;
    uint32_t interrupter_target:10;  /* Interrupter Target (0-1023) */
    
    /* Word 5 */
    uint32_t usb_device_address:8;   /* USB Device Address */
    uint32_t reserved6:19;
    uint32_t slot_state:5;           /* Slot State */
    
    /* Words 6-7 */
    uint32_t reserved7;
    uint32_t reserved8;
} xhci_slot_context_t;

/**
 * Endpoint Context
 * 8 words (32 bytes) per XHCI 1.2 spec
 * Word 0: EP State [2:0], Reserved [7:3], Mult [9:8], Max P-Streams [14:10], LSA [15], Interval [23:16]
 * Word 1: Max ESIT Payload Hi [7:0], Error Count [9:8], EP Type [12:10], Reserved [15:13], Max Packet Size [31:16]
 * Word 2: Max Burst Size [7:0], Reserved [15:8], Max ESIT Payload Lo [23:16]
 * Word 3: Reserved [15:0], Average TRB Length [31:16]
 * Words 4-5: TR Dequeue Pointer (64-bit, bit 0 = DCS)
 * Word 6: Reserved [15:0], Interrupter Target [25:16]
 * Word 7: Reserved
 */
typedef struct PACKED {
    /* Word 0 */
    uint32_t ep_state:3;             /* Endpoint State */
    uint32_t reserved0:5;
    uint32_t mult:2;                 /* Mult (for isochronous) */
    uint32_t max_pstreams:5;         /* Max Primary Streams */
    uint32_t lsa:1;                  /* Linear Stream Array */
    uint32_t interval:8;             /* Interval (for interrupt/isochronous) */
    uint32_t max_esit_payload_hi:8;  /* Max ESIT Payload High */
    
    /* Word 1 */
    uint32_t reserved1:1;
    uint32_t error_count:2;          /* Error Count */
    uint32_t ep_type:3;              /* Endpoint Type: 1=IsochOut, 2=BulkOut, 3=IntOut, 4=Control, 5=IsochIn, 6=BulkIn, 7=IntIn */
    uint32_t reserved2:1;
    uint32_t max_packet_size:16;     /* Max Packet Size */
    uint32_t max_esit_payload_lo:8;  /* Max ESIT Payload Low */
    
    /* Word 2 */
    uint32_t reserved3:8;
    uint32_t max_burst_size:8;       /* Max Burst Size */
    uint32_t reserved4:8;
    uint32_t max_esit_payload_mid:8; /* Max ESIT Payload Middle */
    
    /* Word 3 */
    uint32_t reserved5:16;
    uint32_t average_trb_length:16; /* Average TRB Length */
    
    /* Words 4-5: TR Dequeue Pointer (64-bit) */
    uint64_t dequeue_pointer;        /* Physical address of TRB, bit 0 = DCS (Dequeue Cycle State) */
    
    /* Word 6 */
    uint32_t reserved6:16;
    uint32_t interrupter_target:10;  /* Interrupter Target (0-1023) */
    uint32_t reserved7:6;
    
    /* Word 7 */
    uint32_t reserved8;
} xhci_endpoint_context_t;

/**
 * Input Context
 * Complete Input Context structure for Address Device command
 * Size: 64 bytes (Input Control Context) + 32 bytes (Slot Context) + 32*31 bytes (Endpoint Contexts) = 1056 bytes
 * We use a simplified version with space for all 31 endpoints
 */
typedef struct PACKED {
    /* Input Control Context (32 bytes) */
    xhci_input_control_context_t control;
    
    /* Slot Context (32 bytes) */
    xhci_slot_context_t slot;
    
    /* Endpoint Contexts (32 bytes each, up to 31 endpoints) */
    xhci_endpoint_context_t endpoints[31];
} xhci_input_context_t;

/**
 * Device Context (Output Context)
 * This is what gets stored in DCBAAP[slot_id]
 * Size: 32 bytes (Slot Context) + 32*N bytes (Endpoint Contexts)
 * Minimum: 32 + 32 = 64 bytes for EP0 only
 * Maximum: 32 + 32*31 = 1024 bytes for all endpoints
 */
typedef struct PACKED {
    /* Slot Context (32 bytes) */
    xhci_slot_context_t slot;
    
    /* Endpoint Contexts (32 bytes each, up to 31 endpoints) */
    xhci_endpoint_context_t endpoints[31];
} xhci_device_context_t;

/* Verify structure sizes at compile time */
#ifndef __cplusplus
_Static_assert(sizeof(xhci_input_control_context_t) == 32, "Input Control Context must be 32 bytes");
_Static_assert(sizeof(xhci_slot_context_t) == 32, "Slot Context must be 32 bytes");
_Static_assert(sizeof(xhci_endpoint_context_t) == 32, "Endpoint Context must be 32 bytes");
_Static_assert(sizeof(xhci_input_context_t) >= 64, "Input Context must be at least 64 bytes");
_Static_assert(sizeof(xhci_device_context_t) >= 64, "Device Context must be at least 64 bytes");
#endif

/* XHCI Context Functions */
xhci_input_context_t *xhci_input_context_alloc(void);
void xhci_input_context_free(xhci_input_context_t *ctx);
void xhci_input_context_set_slot(xhci_input_context_t *ctx, uint8_t slot_id, uint8_t root_port,
                                 uint8_t speed, uint8_t address, bool hub, uint8_t parent_slot,
                                 uint8_t parent_port);
/* Note: xhci_transfer_ring_t is defined in xhci.h, we use forward declaration here */
void xhci_input_context_set_ep0(xhci_input_context_t *ctx, struct xhci_transfer_ring *transfer_ring,
                                 uint16_t max_packet_size);
phys_addr_t xhci_input_context_get_phys(xhci_input_context_t *ctx);

#ifdef __cplusplus
}
#endif

