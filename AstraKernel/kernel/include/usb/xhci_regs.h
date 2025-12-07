#pragma once

#include "types.h"

/**
 * XHCI Register Structures
 * 
 * Hardware register definitions according to XHCI 1.2 specification.
 * 
 * CRITICAL: All structures MUST be PACKED and all fields MUST be volatile
 * to ensure correct memory layout matching hardware MMIO registers.
 * 
 * Offsets verified against XHCI 1.2 specification:
 * - Capability Registers: 0x00-0x1F
 * - Operational Registers: 0x00-0x3F (relative to op_regs base)
 * - Runtime Registers: per interrupter, 0x00-0x3F (relative to rt_regs base)
 */

/* XHCI Capability Registers Structure */
/* Base address: MMIO base (cap_regs) */
typedef struct PACKED {
    volatile uint8_t  CAPLENGTH;        // 0x00 - Capability Length
    volatile uint8_t  RESERVED;         // 0x01
    volatile uint16_t HCIVERSION;       // 0x02 - Interface Version Number
    volatile uint32_t HCSPARAMS1;       // 0x04 - Structural Parameters 1
    volatile uint32_t HCSPARAMS2;       // 0x08 - Structural Parameters 2
    volatile uint32_t HCSPARAMS3;       // 0x0C - Structural Parameters 3
    volatile uint32_t HCCPARAMS1;       // 0x10 - Capability Parameters 1
    volatile uint32_t DBOFF;            // 0x14 - Doorbell Offset
    volatile uint32_t RTSOFF;           // 0x18 - Runtime Register Space Offset
    volatile uint32_t HCCPARAMS2;       // 0x1C - Capability Parameters 2 (if supported)
} xhci_cap_regs_t;

/* XHCI Operational Registers Structure */
/* Base address: cap_regs + CAPLENGTH */
/* CRITICAL: CRCR MUST be at offset 0x18 (64-bit register) */
typedef struct __attribute__((aligned(8))) {
    volatile uint32_t USBCMD;      // 0x00 - USB Command
    volatile uint32_t USBSTS;      // 0x04 - USB Status
    volatile uint32_t PAGESIZE;    // 0x08 - Page Size
    uint32_t rsvd0;                // 0x0C

    volatile uint32_t DNCTRL;      // 0x10 - Device Notification Control
    uint32_t rsvd1;                // 0x14

    volatile uint64_t CRCR;        // 0x18 - Command Ring Control Register (64-bit)

    uint32_t rsvd2;                // 0x20
    uint32_t rsvd3;                // 0x24
    uint32_t rsvd4;                // 0x28
    uint32_t rsvd5;                // 0x2C

    volatile uint64_t DCBAAP;      // 0x30 - Device Context Base Address Array Pointer
    volatile uint32_t CONFIG;      // 0x38 - Configure
    uint32_t rsvd6;                // 0x3C
} xhci_op_regs_t;

/* XHCI Interrupter Registers Structure (per interrupter) */
/* Each interrupter is 0x20 bytes (32 bytes) */
/* Offsets relative to start of interrupter register block */
typedef struct PACKED {
    volatile uint32_t iman;      // 0x00 - Interrupter Management
    volatile uint32_t imod;      // 0x04 - Interrupter Moderation
    volatile uint32_t erstsz;    // 0x08 - Event Ring Segment Table Size
    volatile uint32_t rsvd;      // 0x0C - Reserved
    volatile uint64_t erstba;    // 0x10 - Event Ring Segment Table Base Address
    volatile uint64_t erdp;      // 0x18 - Event Ring Dequeue Pointer
} xhci_interrupter_regs_t;

/* XHCI Runtime Registers Structure */
/* Base address: cap_regs + RTSOFF */
/* Correct layout per XHCI 1.2 spec:
 * - mfindex at 0x0
 * - 7 reserved uint32_t at 0x4-0x20
 * - Interrupter 0 starts at 0x20
 * - Each interrupter is 0x20 bytes
 */
typedef struct PACKED {
    volatile uint32_t mfindex;   // 0x0 - Microframe Index
    volatile uint32_t rsvd[7];   // 0x4 - 0x20 - Reserved
    xhci_interrupter_regs_t ir[1]; // 0x20 - Interrupter 0 (and more if supported)
} xhci_rt_regs_t;

/* XHCI Doorbell Registers Structure */
/* Base address: cap_regs + DBOFF */
/* Each doorbell is 32-bit: [31:16] = Stream ID, [15:8] = Endpoint ID, [7:0] = Slot ID */
/* Doorbell 0 = Command doorbell (slot 0, endpoint 0) */
typedef struct PACKED {
    volatile uint32_t doorbell[256];    // Doorbell registers (up to 256 slots)
} xhci_doorbell_regs_t;
