#pragma once

#include "types.h"
#include <stdint.h>

/* Full Capability Registers Structure */
typedef struct PACKED {
    uint8_t  CAPLENGTH;           /* 0x00 - Capability Register Length */
    uint8_t  reserved1;
    uint16_t HCIVERSION;          /* 0x02 - Interface Version Number */
    uint32_t HCSPARAMS1;          /* 0x04 - Structural Parameters 1 */
    uint32_t HCSPARAMS2;          /* 0x08 - Structural Parameters 2 */
    uint32_t HCSPARAMS3;          /* 0x0C - Structural Parameters 3 */
    uint32_t HCCPARAMS1;          /* 0x10 - Capability Parameters 1 */
    uint32_t DBOFF;               /* 0x14 - Doorbell Offset */
    uint32_t RTSOFF;              /* 0x18 - Runtime Register Space Offset */
    uint32_t HCCPARAMS2;          /* 0x1C - Capability Parameters 2 */
} xhci_cap_regs_t;

/* HCSPARAMS1 bit fields */
#define XHCI_HCS1_MAX_SLOTS_MASK    0xFF
#define XHCI_HCS1_MAX_INTRS_SHIFT   8
#define XHCI_HCS1_MAX_INTRS_MASK    0x7FF
#define XHCI_HCS1_MAX_PORTS_SHIFT   24
#define XHCI_HCS1_MAX_PORTS_MASK    0xFF

/* HCSPARAMS2 bit fields */
#define XHCI_HCS2_IST_MASK          0xF
#define XHCI_HCS2_ERST_MAX_SHIFT    4
#define XHCI_HCS2_ERST_MAX_MASK     0xF
#define XHCI_HCS2_SPR               (1 << 26)
#define XHCI_HCS2_MAX_SCRATCH_HI_SHIFT 21
#define XHCI_HCS2_MAX_SCRATCH_HI_MASK 0x1F
#define XHCI_HCS2_MAX_SCRATCH_LO_SHIFT 27
#define XHCI_HCS2_MAX_SCRATCH_LO_MASK 0x1F

/* HCCPARAMS1 bit fields */
#define XHCI_HCC1_AC64              (1 << 0)
#define XHCI_HCC1_BNC               (1 << 1)
#define XHCI_HCC1_CSZ               (1 << 2)
#define XHCI_HCC1_PPC               (1 << 3)
#define XHCI_HCC1_PIND              (1 << 4)
#define XHCI_HCC1_LHRC              (1 << 5)
#define XHCI_HCC1_LTC               (1 << 6)
#define XHCI_HCC1_NSS               (1 << 7)
#define XHCI_HCC1_PAE               (1 << 8)
#define XHCI_HCC1_SPC               (1 << 9)
#define XHCI_HCC1_SEC               (1 << 10)
#define XHCI_HCC1_CFC               (1 << 11)
#define XHCI_HCC1_MAX_PSA_SHIFT     12
#define XHCI_HCC1_MAX_PSA_MASK      0xF
#define XHCI_HCC1_EXT_CAP_PTR_SHIFT 16
#define XHCI_HCC1_EXT_CAP_PTR_MASK  0xFFFF

/* Operational registers */
typedef struct PACKED {
    uint32_t USBCMD;              /* 0x00 - USB Command */
    uint32_t USBSTS;              /* 0x04 - USB Status */
    uint32_t PAGESIZE;            /* 0x08 - Page Size */
    uint32_t reserved1[2];        /* 0x0C-0x10 */
    uint32_t DNCTRL;              /* 0x14 - Device Notification Control */
    uint64_t CRCR;                /* 0x18 - Command Ring Control Register */
    uint32_t reserved2[4];        /* 0x20-0x2C */
    uint64_t DCBAAP;              /* 0x30 - Device Context Base Address Array Pointer */
    uint32_t CONFIG;              /* 0x38 - Configure */
} xhci_op_regs_t;

/* Port register set (repeated per port) */
typedef struct PACKED {
    uint32_t PORTSC;              /* Port Status and Control */
    uint32_t PORTPMSC;            /* Port Power Management Status and Control */
    uint32_t PORTLI;              /* Port Link Info */
    uint32_t PORTHLPMC;           /* Port Hardware LPM Control */
} xhci_port_regs_t;

/* Interrupter register set */
typedef struct PACKED {
    uint32_t IMAN;                /* 0x00 - Interrupter Management */
    uint32_t IMOD;                /* 0x04 - Interrupter Moderation */
    uint32_t ERSTSZ;              /* 0x08 - Event Ring Segment Table Size */
    uint32_t reserved;            /* 0x0C */
    uint64_t ERSTBA;              /* 0x10 - Event Ring Segment Table Base Address */
    uint64_t ERDP;                /* 0x18 - Event Ring Dequeue Pointer */
} xhci_intr_regset_t;

/* Runtime registers */
typedef struct PACKED {
    uint32_t MFINDEX;             /* 0x00 - Microframe Index */
    uint32_t reserved[7];         /* 0x04-0x1F */
    xhci_intr_regset_t ir[1024];  /* 0x20+ - Interrupter Register Sets */
} xhci_rt_regs_t;

/* Doorbell array */
typedef struct PACKED {
    uint32_t doorbell[256];       /* Doorbell Register array */
} xhci_doorbell_regs_t;

/* Doorbell register bit fields */
#define XHCI_DB_TARGET_SHIFT        0
#define XHCI_DB_TARGET_MASK         0xFF
#define XHCI_DB_STREAM_ID_SHIFT     16
#define XHCI_DB_STREAM_ID_MASK      0xFFFF

/* Extended capabilities header */
typedef struct PACKED {
    uint32_t capability_id:8;     /* Capability ID */
    uint32_t next_ptr:8;          /* Next Capability Pointer */
    uint32_t cap_specific:16;     /* Capability Specific */
} xhci_extended_cap_header_t;

/* Extended Capability IDs */
#define XHCI_EXT_CAP_USBLEGSUP      1
#define XHCI_EXT_CAP_PROTOCOL       2
#define XHCI_EXT_CAP_EXT_POWER_MGMT 3
#define XHCI_EXT_CAP_IOVIRT         4
#define XHCI_EXT_CAP_MSI            5
#define XHCI_EXT_CAP_LOCALMEM       6
#define XHCI_EXT_CAP_DEBUG          10
#define XHCI_EXT_CAP_EXT_MSG_INTR   17

/* USB Legacy Support Capability */
typedef struct PACKED {
    uint32_t capability_id:8;
    uint32_t next_ptr:8;
    uint32_t bios_owned:1;
    uint32_t reserved:7;
    uint32_t os_owned:1;
    uint32_t reserved2:7;
    uint32_t control_status;
} xhci_usb_legacy_cap_t;

#define XHCI_USBLEGSUP_BIOS_OWNED   (1 << 16)
#define XHCI_USBLEGSUP_OS_OWNED     (1 << 24)

