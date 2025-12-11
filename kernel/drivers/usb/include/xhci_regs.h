#ifndef XHCI_REGS_H
#define XHCI_REGS_H

#include <stdint.h>

/* Minimal xHCI register structures for later expansion. */
typedef struct xhci_capability_regs {
    volatile uint8_t caplength;
    volatile uint8_t reserved;
    volatile uint16_t hciversion;
    volatile uint32_t hcsparams1;
    volatile uint32_t hcsparams2;
    volatile uint32_t hcsparams3;
    volatile uint32_t hccparams1;
    volatile uint32_t dboff;
    volatile uint32_t rtsoff;
    volatile uint32_t hccparams2;
} xhci_capability_regs_t;

typedef struct xhci_operational_regs {
    volatile uint32_t usbcmd;
    volatile uint32_t usbsts;
    volatile uint32_t pagesize;
    volatile uint32_t reserved[2];
    volatile uint32_t dnctrl;
    volatile uint64_t crcr;
    volatile uint64_t dcbaap;
    volatile uint32_t config;
} xhci_operational_regs_t;

#define XHCI_USBCMD_RUN   (1u << 0)
#define XHCI_USBCMD_HCRST (1u << 1)

typedef struct xhci_runtime_regs {
    volatile uint32_t mfindex;
    struct {
        volatile uint32_t iman;
        volatile uint32_t imod;
        volatile uint32_t erstsz;
        volatile uint32_t reserved;
        volatile uint64_t erstba;
        volatile uint64_t erdp;
    } intr[1];
} xhci_runtime_regs_t;

#endif /* XHCI_REGS_H */
