/**
 * EHCI Transfer Descriptor Management
 * 
 * Implements transfer descriptor allocation and linking for EHCI transfers.
 */

#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "pmm.h"

/* EHCI Transfer Descriptor Structure (32 bytes, 32-byte aligned) */
typedef struct PACKED {
    uint32_t td_link;
    uint32_t td_token;
    uint32_t td_buffer[2];
    uint32_t td_buffer_hi[2];
    uint32_t td_extend;
} ehci_td_t;

/* Transfer Descriptor Link Pointer */
#define EHCI_TD_LINK_TERMINATE    (1 << 0)
#define EHCI_TD_LINK_TYPE_ITD     (2 << 1)
#define EHCI_TD_LINK_TYPE_SITD    (3 << 1)
#define EHCI_TD_LINK_TYPE_FSTN    (4 << 1)

/* Transfer Descriptor Token */
#define EHCI_TD_TOKEN_STATUS_SHIFT 0
#define EHCI_TD_TOKEN_PID_SHIFT    2
#define EHCI_TD_TOKEN_CERR_SHIFT  10
#define EHCI_TD_TOKEN_CPAGE_SHIFT 12
#define EHCI_TD_TOKEN_IOC_SHIFT   15
#define EHCI_TD_TOKEN_LENGTH_SHIFT 16
#define EHCI_TD_TOKEN_MULT_SHIFT  21

/* Transfer Descriptor Status */
#define EHCI_TD_STATUS_ACTIVE     (1 << 0)
#define EHCI_TD_STATUS_HALTED     (1 << 1)
#define EHCI_TD_STATUS_DATA_BUFFER_ERROR (1 << 2)
#define EHCI_TD_STATUS_BABBLE     (1 << 3)
#define EHCI_TD_STATUS_XACT_ERROR (1 << 4)
#define EHCI_TD_STATUS_MISSED_MF  (1 << 5)
#define EHCI_TD_STATUS_SPLIT_XACT_STATE_SHIFT 6
#define EHCI_TD_STATUS_PING_STATE (1 << 8)

/* PID Types */
#define EHCI_TD_PID_OUT           0
#define EHCI_TD_PID_IN            1
#define EHCI_TD_PID_SETUP         2

/**
 * Allocate a transfer descriptor
 */
ehci_td_t *ehci_td_alloc(void) {
    ehci_td_t *td = (ehci_td_t *)kmalloc(sizeof(ehci_td_t) + 32);
    if (!td) {
        klog_printf(KLOG_ERROR, "ehci_td: allocation failed");
        return NULL;
    }
    
    /* Align to 32-byte boundary */
    uintptr_t addr = (uintptr_t)td;
    if (addr % 32 != 0) {
        addr = (addr + 31) & ~31;
        td = (ehci_td_t *)addr;
    }
    
    k_memset(td, 0, sizeof(ehci_td_t));
    
    /* Initialize link pointer to terminate */
    td->td_link = EHCI_TD_LINK_TERMINATE;
    
    return td;
}

/**
 * Free a transfer descriptor
 */
void ehci_td_free(ehci_td_t *td) {
    if (!td) return;
    kfree(td);
}

/**
 * Get physical address of transfer descriptor
 */
phys_addr_t ehci_td_get_phys(ehci_td_t *td) {
    if (!td) return 0;
    /* TODO: Get physical address from virtual address */
    return (phys_addr_t)(uintptr_t)td;
}

/**
 * Initialize transfer descriptor
 */
void ehci_td_init(ehci_td_t *td, uint8_t pid, uint32_t buffer_addr, 
                  uint16_t length, uint8_t toggle, bool ioc) {
    if (!td) return;
    
    /* Token */
    uint32_t token = 0;
    token |= EHCI_TD_STATUS_ACTIVE; /* Active */
    token |= ((uint32_t)pid << EHCI_TD_TOKEN_PID_SHIFT);
    token |= ((uint32_t)toggle << EHCI_TD_TOKEN_CERR_SHIFT);
    if (ioc) {
        token |= (1 << EHCI_TD_TOKEN_IOC_SHIFT);
    }
    token |= ((uint32_t)length << EHCI_TD_TOKEN_LENGTH_SHIFT);
    
    td->td_token = token;
    
    /* Buffer pointers */
    td->td_buffer[0] = buffer_addr;
    td->td_buffer[1] = 0;
    td->td_buffer_hi[0] = 0;
    td->td_buffer_hi[1] = 0;
}

/**
 * Link transfer descriptors
 */
void ehci_td_link(ehci_td_t *prev, ehci_td_t *next) {
    if (!prev) return;
    
    if (!next) {
        prev->td_link = EHCI_TD_LINK_TERMINATE;
    } else {
        phys_addr_t next_phys = ehci_td_get_phys(next);
        prev->td_link = (uint32_t)next_phys | EHCI_TD_LINK_TERMINATE;
    }
}

/**
 * Check if transfer descriptor completed successfully
 */
bool ehci_td_completed(ehci_td_t *td) {
    if (!td) return false;
    
    uint32_t status = td->td_token & 0xFF;
    
    /* Check if active bit is cleared */
    if (status & EHCI_TD_STATUS_ACTIVE) {
        return false; /* Still active */
    }
    
    /* Check for errors */
    if (status & (EHCI_TD_STATUS_HALTED | EHCI_TD_STATUS_DATA_BUFFER_ERROR |
                  EHCI_TD_STATUS_BABBLE | EHCI_TD_STATUS_XACT_ERROR)) {
        return false; /* Error occurred */
    }
    
    return true; /* Completed successfully */
}

/**
 * Get actual length transferred
 */
uint16_t ehci_td_get_length(ehci_td_t *td) {
    if (!td) return 0;
    
    uint32_t token = td->td_token;
    uint16_t total_length = (token >> EHCI_TD_TOKEN_LENGTH_SHIFT) & 0x7FFF;
    uint16_t actual_length = (token >> EHCI_TD_TOKEN_STATUS_SHIFT) & 0x7FFF;
    
    return total_length - actual_length;
}

