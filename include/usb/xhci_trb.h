#pragma once

#include "types.h"
#include <stdbool.h>
#include <stdint.h>

/* TRB Types */
#define XHCI_TRB_TYPE_NORMAL             1
#define XHCI_TRB_TYPE_SETUP_STAGE        2
#define XHCI_TRB_TYPE_DATA_STAGE         3
#define XHCI_TRB_TYPE_STATUS_STAGE       4
#define XHCI_TRB_TYPE_ISOCH              5
#define XHCI_TRB_TYPE_LINK               6
#define XHCI_TRB_TYPE_EVENT_DATA         7
#define XHCI_TRB_TYPE_NOOP               8
#define XHCI_TRB_TYPE_ENABLE_SLOT        9
#define XHCI_TRB_TYPE_DISABLE_SLOT       10
#define XHCI_TRB_TYPE_ADDRESS_DEVICE     11
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT 12
#define XHCI_TRB_TYPE_EVALUATE_CONTEXT   13
#define XHCI_TRB_TYPE_RESET_ENDPOINT     14
#define XHCI_TRB_TYPE_STOP_ENDPOINT      15
#define XHCI_TRB_TYPE_SET_TR_DEQUEUE     16
#define XHCI_TRB_TYPE_RESET_DEVICE       17
#define XHCI_TRB_TYPE_FORCE_EVENT        18
#define XHCI_TRB_TYPE_NEGOTIATE_BANDWIDTH 19
#define XHCI_TRB_TYPE_SET_LATENCY_TOLERANCE 20
#define XHCI_TRB_TYPE_GET_PORT_BANDWIDTH 21
#define XHCI_TRB_TYPE_FORCE_HEADER       22
#define XHCI_TRB_TYPE_NOOP_CMD           23
#define XHCI_TRB_TYPE_GET_EXT_PROPERTY   24
#define XHCI_TRB_TYPE_SET_EXT_PROPERTY   25

/* Event TRB Types */
#define XHCI_TRB_EVENT_TRANSFER          32
#define XHCI_TRB_EVENT_CMD_COMPLETION    33
#define XHCI_TRB_EVENT_PORT_STATUS       34
#define XHCI_TRB_EVENT_BANDWIDTH_REQUEST 35
#define XHCI_TRB_EVENT_DOORBELL          36
#define XHCI_TRB_EVENT_HOST_CONTROLLER   37
#define XHCI_TRB_EVENT_DEVICE_NOTIFICATION 38
#define XHCI_TRB_EVENT_MFINDEX_WRAP      39

/* TRB Completion Codes */
#define XHCI_TRB_CC_INVALID                    0
#define XHCI_TRB_CC_SUCCESS                    1
#define XHCI_TRB_CC_DATA_BUFFER_ERROR          2
#define XHCI_TRB_CC_BABBLE_DETECTED            3
#define XHCI_TRB_CC_USB_TRANSACTION_ERROR      4
#define XHCI_TRB_CC_TRB_ERROR                  5
#define XHCI_TRB_CC_STALL_ERROR                6
#define XHCI_TRB_CC_RESOURCE_ERROR             7
#define XHCI_TRB_CC_BANDWIDTH_ERROR            8
#define XHCI_TRB_CC_NO_SLOTS_AVAILABLE         9
#define XHCI_TRB_CC_INVALID_STREAM_TYPE        10
#define XHCI_TRB_CC_SLOT_NOT_ENABLED           11
#define XHCI_TRB_CC_ENDPOINT_NOT_ENABLED       12
#define XHCI_TRB_CC_SHORT_PACKET               13
#define XHCI_TRB_CC_RING_UNDERRUN              14
#define XHCI_TRB_CC_RING_OVERRUN               15
#define XHCI_TRB_CC_VF_EVENT_RING_FULL         16
#define XHCI_TRB_CC_PARAMETER_ERROR            17
#define XHCI_TRB_CC_BANDWIDTH_OVERRUN          18
#define XHCI_TRB_CC_CONTEXT_STATE_ERROR        19
#define XHCI_TRB_CC_NO_PING_RESPONSE           20
#define XHCI_TRB_CC_EVENT_RING_FULL            21
#define XHCI_TRB_CC_INCOMPATIBLE_DEVICE        22
#define XHCI_TRB_CC_MISSED_SERVICE_ERROR       23
#define XHCI_TRB_CC_COMMAND_RING_STOPPED       24
#define XHCI_TRB_CC_COMMAND_ABORTED            25
#define XHCI_TRB_CC_STOPPED                    26
#define XHCI_TRB_CC_STOPPED_LENGTH_INVALID     27
#define XHCI_TRB_CC_STOPPED_SHORT_PACKET       28
#define XHCI_TRB_CC_MAX_EXIT_LATENCY_TOO_LARGE 29
#define XHCI_TRB_CC_ISOCH_BUFFER_OVERRUN       31
#define XHCI_TRB_CC_EVENT_LOST_ERROR           32
#define XHCI_TRB_CC_UNDEFINED_ERROR            33
#define XHCI_TRB_CC_INVALID_STREAM_ID_ERROR    34
#define XHCI_TRB_CC_SECONDARY_BANDWIDTH_ERROR  35
#define XHCI_TRB_CC_SPLIT_TRANSACTION_ERROR    36

/* TRB control field bits */
#define XHCI_TRB_CYCLE           (1 << 0)
#define XHCI_TRB_ENT             (1 << 1)   /* Evaluate Next TRB */
#define XHCI_TRB_TC              XHCI_TRB_ENT /* Alias for link TRB Toggle Cycle */
#define XHCI_TRB_ISP             (1 << 2)   /* Interrupt on Short Packet */
#define XHCI_TRB_NS              (1 << 3)   /* No Snoop */
#define XHCI_TRB_CH              (1 << 4)   /* Chain bit */
#define XHCI_TRB_IOC             (1 << 5)   /* Interrupt On Completion */
#define XHCI_TRB_IDT             (1 << 6)   /* Immediate Data */
#define XHCI_TRB_BEI             (1 << 9)   /* Block Event Interrupt */
#define XHCI_TRB_TYPE_SHIFT      10
#define XHCI_TRB_TYPE_MASK       0x3F
#define XHCI_TRB_TRT_SHIFT       16         /* Transfer Type */
#define XHCI_TRB_TRT_MASK        0x30000
#define XHCI_TRB_DIR_IN          (1 << 16)  /* Direction */
#define XHCI_TRB_TLBPC_SHIFT     16         /* Transfer Last Burst Packet Count */
#define XHCI_TRB_FRAME_ID_SHIFT  20
#define XHCI_TRB_SIA             (1 << 31)  /* Set Interrupter Target */

/* Transfer Request Block types */
#define XHCI_TRT_NO_DATA         0
#define XHCI_TRT_OUT_DATA        2
#define XHCI_TRT_IN_DATA         3

/* TRB Structure (16 bytes) */
typedef struct PACKED {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

/* Event TRB */
typedef struct PACKED {
    uint64_t data;
    uint32_t status;
    uint32_t control;
} xhci_event_trb_t;

/* Event TRB field helpers */
#define XHCI_EVENT_TRB_TRANSFER_LENGTH_SHIFT   0
#define XHCI_EVENT_TRB_TRANSFER_LENGTH_MASK    0x00FFFFFF
#define XHCI_EVENT_TRB_COMPLETION_CODE_SHIFT   24
#define XHCI_EVENT_TRB_COMPLETION_CODE_MASK    0xFF
#define XHCI_EVENT_TRB_CYCLE_BIT               (1u << 0)
#define XHCI_EVENT_TRB_ENDPOINT_ID_SHIFT       16
#define XHCI_EVENT_TRB_ENDPOINT_ID_MASK        0x1F
#define XHCI_EVENT_TRB_SLOT_ID_SHIFT           24
#define XHCI_EVENT_TRB_SLOT_ID_MASK            0xFF
#define XHCI_EVENT_TRB_TYPE_SHIFT              10
#define XHCI_EVENT_TRB_TYPE_MASK               0x3F

/* Command Completion Event TRB */
typedef struct PACKED {
    uint64_t command_trb_pointer;
    uint32_t completion_code:8;
    uint32_t reserved:16;
    uint32_t vf_id:8;
    uint32_t slot_id:8;
    uint32_t reserved2:16;
    uint32_t trb_type:6;
    uint32_t cycle:1;
    uint32_t reserved3:1;
} xhci_cmd_completion_event_t;

/* Transfer Event TRB */
typedef struct PACKED {
    uint64_t trb_pointer;
    uint32_t transfer_length:24;
    uint32_t completion_code:8;
    uint32_t cycle:1;
    uint32_t reserved:1;
    uint32_t ed:1;
    uint32_t reserved2:7;
    uint32_t trb_type:6;
    uint32_t endpoint_id:5;
    uint32_t reserved3:3;
    uint32_t slot_id:8;
} xhci_transfer_event_t;

/* Port Status Change Event TRB */
typedef struct PACKED {
    uint32_t reserved:24;
    uint32_t port_id:8;
    uint32_t reserved2;
    uint32_t reserved3:24;
    uint32_t completion_code:8;
    uint32_t cycle:1;
    uint32_t reserved4:9;
    uint32_t trb_type:6;
    uint32_t reserved5:16;
} xhci_port_status_event_t;

