/**
 * EHCI Queue Head Management
 * 
 * Implements queue head allocation, linking, and management for EHCI transfers.
 */

#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "pmm.h"

/* EHCI Queue Head Structure (32 bytes, 32-byte aligned) */
typedef struct PACKED {
    uint32_t qh_link;
    uint32_t qh_endp_cap;
    uint32_t qh_endp_char;
    uint32_t qh_endp_cap_hi;
    uint32_t qh_cur_td;
    uint32_t qh_next_td;
    uint32_t qh_alt_td;
    uint32_t qh_token;
} ehci_qh_t;

/* Queue Head Link Pointer */
#define EHCI_QH_LINK_TERMINATE   (1 << 0)
#define EHCI_QH_LINK_TYPE_QH     (1 << 1)
#define EHCI_QH_LINK_TYPE_ITD    (2 << 1)
#define EHCI_QH_LINK_TYPE_SITD   (3 << 1)
#define EHCI_QH_LINK_TYPE_FSTN   (4 << 1)

/* Queue Head Endpoint Characteristics */
#define EHCI_QH_EPCAP_MULT_SHIFT 30
#define EHCI_QH_EPCAP_PORT_SHIFT 23
#define EHCI_QH_EPCAP_HUB_ADDR_SHIFT 16
#define EHCI_QH_EPCAP_ADDR_SHIFT 8
#define EHCI_QH_EPCAP_IN_SHIFT  7
#define EHCI_QH_EPCAP_EP_TYPE_SHIFT 3
#define EHCI_QH_EPCAP_MAX_PACKET_SHIFT 16

/* Queue Head Endpoint Capabilities */
#define EHCI_QH_EPCAP_C_MASK     (1 << 0)
#define EHCI_QH_EPCAP_S_MASK     (1 << 1)
#define EHCI_QH_EPCAP_H_MASK     (1 << 2)
#define EHCI_QH_EPCAP_DTC        (1 << 3)
#define EHCI_QH_EPCAP_NRL_SHIFT  20

/**
 * Allocate a queue head
 */
ehci_qh_t *ehci_qh_alloc(void) {
    ehci_qh_t *qh = (ehci_qh_t *)kmalloc(sizeof(ehci_qh_t) + 32);
    if (!qh) {
        klog_printf(KLOG_ERROR, "ehci_qh: allocation failed");
        return NULL;
    }
    
    /* Align to 32-byte boundary */
    uintptr_t addr = (uintptr_t)qh;
    if (addr % 32 != 0) {
        addr = (addr + 31) & ~31;
        qh = (ehci_qh_t *)addr;
    }
    
    k_memset(qh, 0, sizeof(ehci_qh_t));
    
    /* Initialize link pointer to terminate */
    qh->qh_link = EHCI_QH_LINK_TERMINATE;
    
    return qh;
}

/**
 * Free a queue head
 */
void ehci_qh_free(ehci_qh_t *qh) {
    if (!qh) return;
    kfree(qh);
}

/**
 * Get physical address of queue head
 */
phys_addr_t ehci_qh_get_phys(ehci_qh_t *qh) {
    if (!qh) return 0;
    /* TODO: Get physical address from virtual address */
    return (phys_addr_t)(uintptr_t)qh;
}

/**
 * Initialize queue head for endpoint
 */
void ehci_qh_init(ehci_qh_t *qh, uint8_t device_addr, uint8_t endpoint, 
                  uint8_t endpoint_type, uint16_t max_packet_size, 
                  uint8_t hub_addr, uint8_t port, uint8_t speed) {
    if (!qh) return;
    
    /* Endpoint Characteristics */
    uint32_t ep_char = 0;
    ep_char |= (1 << EHCI_QH_EPCAP_MULT_SHIFT); /* Multi = 1 */
    ep_char |= ((uint32_t)port << EHCI_QH_EPCAP_PORT_SHIFT);
    ep_char |= ((uint32_t)hub_addr << EHCI_QH_EPCAP_HUB_ADDR_SHIFT);
    ep_char |= ((uint32_t)device_addr << EHCI_QH_EPCAP_ADDR_SHIFT);
    ep_char |= ((uint32_t)endpoint << EHCI_QH_EPCAP_IN_SHIFT);
    ep_char |= ((uint32_t)endpoint_type << EHCI_QH_EPCAP_EP_TYPE_SHIFT);
    ep_char |= ((uint32_t)max_packet_size << EHCI_QH_EPCAP_MAX_PACKET_SHIFT);
    
    qh->qh_endp_cap = ep_char;
    
    /* Endpoint Capabilities */
    uint32_t ep_cap = 0;
    if (speed == USB_SPEED_HIGH) {
        ep_cap |= EHCI_QH_EPCAP_C_MASK; /* C = 1 for high speed */
    }
    ep_cap |= EHCI_QH_EPCAP_S_MASK; /* S = 1 */
    ep_cap |= EHCI_QH_EPCAP_DTC; /* Data toggle control */
    
    qh->qh_endp_cap_hi = ep_cap;
    
    /* Initialize token */
    qh->qh_token = 0;
}

/**
 * Link queue heads
 */
void ehci_qh_link(ehci_qh_t *prev, ehci_qh_t *next) {
    if (!prev) return;
    
    if (!next) {
        prev->qh_link = EHCI_QH_LINK_TERMINATE;
    } else {
        phys_addr_t next_phys = ehci_qh_get_phys(next);
        prev->qh_link = (uint32_t)next_phys | EHCI_QH_LINK_TYPE_QH;
    }
}

