#pragma once

#include <stdint.h>

/* Capability register offsets */
#define XHCI_CAPLENGTH           0x00
#define XHCI_HCIVERSION          0x02
#define XHCI_HCSPARAMS1          0x04
#define XHCI_HCSPARAMS2          0x08
#define XHCI_HCSPARAMS3          0x0C
#define XHCI_HCCPARAMS1          0x10
#define XHCI_DBOFF               0x14
#define XHCI_RTSOFF              0x18
#define XHCI_HCCPARAMS2          0x1C

/* Operational register offsets */
#define XHCI_USBCMD              0x00
#define XHCI_USBSTS              0x04
#define XHCI_PAGESIZE            0x08
#define XHCI_DNCTRL              0x14
#define XHCI_CRCR                0x18
#define XHCI_DCBAAP              0x30
#define XHCI_CONFIG              0x38

/* Runtime register offsets */
#define XHCI_MFINDEX             0x00
#define XHCI_IMAN(n)             (0x20 + ((n) * 0x20))
#define XHCI_IMOD(n)             (0x24 + ((n) * 0x20))
#define XHCI_ERSTSZ(n)           (0x28 + ((n) * 0x20))
#define XHCI_ERSTBA(n)           (0x30 + ((n) * 0x20))
#define XHCI_ERDP(n)             (0x38 + ((n) * 0x20))

/* Runtime register bits */
#define XHCI_ERSTSZ_ERST_SZ_MASK 0xFFFF
#define XHCI_ERDP_EHB            (1 << 3)
#define XHCI_ERDP_DESI_MASK      0x7
#define XHCI_CRCR_RCS            (1 << 0)
#define XHCI_CRCR_CS             (1 << 1)
#define XHCI_CRCR_CA             (1 << 2)
#define XHCI_CRCR_CRR            (1 << 3)
#define XHCI_CRCR_CSS            XHCI_CRCR_CS

/* Port register offsets (start at 0x400 within operational space) */
#define XHCI_PORTSC(n)           (0x400 + ((n) * 0x10))
#define XHCI_PORTPMSC(n)         (0x404 + ((n) * 0x10))
#define XHCI_PORTLI(n)           (0x408 + ((n) * 0x10))
#define XHCI_PORTHLPMC(n)        (0x40C + ((n) * 0x10))

/* Command bits (USBCMD) */
#define XHCI_CMD_RUN             (1 << 0)
#define XHCI_CMD_HCRST           (1 << 1)
#define XHCI_CMD_INTE            (1 << 2)
#define XHCI_CMD_HSEE            (1 << 3)
#define XHCI_CMD_LHCRST          (1 << 7)
#define XHCI_CMD_CSS             (1 << 8)
#define XHCI_CMD_CRS             (1 << 9)
#define XHCI_CMD_EWE             (1 << 10)
#define XHCI_CMD_EU3S            (1 << 11)
#define XHCI_CMD_CME             (1 << 13)
#define XHCI_CMD_ETE             (1 << 14)
#define XHCI_CMD_TSC_EN          (1 << 15)
#define XHCI_CMD_VTIOE           (1 << 16)

/* Status bits (USBSTS) */
#define XHCI_STS_HCH             (1 << 0)
#define XHCI_STS_HSE             (1 << 2)
#define XHCI_STS_EINT            (1 << 3)
#define XHCI_STS_PCD             (1 << 4)
#define XHCI_STS_SSS             (1 << 8)
#define XHCI_STS_RSS             (1 << 9)
#define XHCI_STS_SRE             (1 << 10)
#define XHCI_STS_CNR             (1 << 11)
#define XHCI_STS_HCE             (1 << 12)

/* Interrupter Management Register (IMAN) */
#define XHCI_IMAN_IP             (1 << 0)   /* Interrupt Pending */
#define XHCI_IMAN_IE             (1 << 1)   /* Interrupt Enable */

