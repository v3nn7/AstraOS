#pragma once

#include <stdint.h>

/* XHCI Speed Codes */
#define XHCI_SPEED_FULL          1
#define XHCI_SPEED_LOW           2
#define XHCI_SPEED_HIGH          3
#define XHCI_SPEED_SUPER         4
#define XHCI_SPEED_SUPER_PLUS    5

/* Port Status and Control (PORTSC) bits */
#define XHCI_PORTSC_CCS          (1 << 0)   /* Current Connect Status */
#define XHCI_PORTSC_PED          (1 << 1)   /* Port Enabled/Disabled */
#define XHCI_PORTSC_OCA          (1 << 3)   /* Over-current Active */
#define XHCI_PORTSC_PR           (1 << 4)   /* Port Reset */
#define XHCI_PORTSC_PLS_SHIFT    5          /* Port Link State */
#define XHCI_PORTSC_PLS_MASK     0x1E0
#define XHCI_PORTSC_PP           (1 << 9)   /* Port Power */
#define XHCI_PORTSC_SPEED_SHIFT  10         /* Port Speed */
#define XHCI_PORTSC_SPEED_MASK   0x3C00
#define XHCI_PORTSC_PIC_SHIFT    14         /* Port Indicator Control */
#define XHCI_PORTSC_PIC_MASK     0xC000
#define XHCI_PORTSC_LWS          (1 << 16)  /* Port Link State Write Strobe */
#define XHCI_PORTSC_CSC          (1 << 17)  /* Connect Status Change */
#define XHCI_PORTSC_PEC          (1 << 18)  /* Port Enabled/Disabled Change */
#define XHCI_PORTSC_WRC          (1 << 19)  /* Warm Port Reset Change */
#define XHCI_PORTSC_OCC          (1 << 20)  /* Over-current Change */
#define XHCI_PORTSC_PRC          (1 << 21)  /* Port Reset Change */
#define XHCI_PORTSC_PLC          (1 << 22)  /* Port Link State Change */
#define XHCI_PORTSC_CEC          (1 << 23)  /* Port Config Error Change */
#define XHCI_PORTSC_CAS          (1 << 24)  /* Cold Attach Status */
#define XHCI_PORTSC_WCE          (1 << 25)  /* Wake on Connect Enable */
#define XHCI_PORTSC_WDE          (1 << 26)  /* Wake on Disconnect Enable */
#define XHCI_PORTSC_WOE          (1 << 27)  /* Wake on Over-current Enable */
#define XHCI_PORTSC_DR           (1 << 30)  /* Device Removable */
#define XHCI_PORTSC_WPR          (1 << 31)  /* Warm Port Reset */
#define XHCI_PORTSC_RW_MASK      0x0FE0FFF0 /* Read/Write bits */

/* Port Link States */
#define XHCI_PLS_U0              0
#define XHCI_PLS_U1              1
#define XHCI_PLS_U2              2
#define XHCI_PLS_U3              3
#define XHCI_PLS_DISABLED        4
#define XHCI_PLS_RXDETECT        5
#define XHCI_PLS_INACTIVE        6
#define XHCI_PLS_POLLING         7
#define XHCI_PLS_RECOVERY        8
#define XHCI_PLS_HOTRESET        9
#define XHCI_PLS_COMPLIANCE      10
#define XHCI_PLS_TESTMODE        11
#define XHCI_PLS_RESUME          15

