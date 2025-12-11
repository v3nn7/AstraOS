/**
 * EHCI Queue Head helpers (minimal, but structurally correct).
 */

#include "kmalloc.h"
#include "klog.h"
#include <stdint.h>
#include <string.h>

typedef struct ehci_qh {
    uint32_t horiz_link;
    uint32_t ep_char;
    uint32_t ep_cap;
    uint32_t cur_qtd;
    uint32_t next_qtd;
    uint32_t alt_next_qtd;
    uint32_t token;
    uint32_t buffer[5];
} ehci_qh_t;

__attribute__((unused)) static ehci_qh_t *ehci_qh_alloc(void) {
    ehci_qh_t *qh = (ehci_qh_t *)kmemalign(64, sizeof(ehci_qh_t));
    if (!qh) {
        klog_printf(KLOG_ERROR, "ehci: qh alloc failed");
        return NULL;
    }
    memset(qh, 0, sizeof(*qh));
    qh->horiz_link = 1; /* T-bit set (invalid) */
    qh->next_qtd = 1;
    qh->alt_next_qtd = 1;
    return qh;
}

__attribute__((unused)) static void ehci_qh_free(ehci_qh_t *qh) {
    if (qh) {
        kfree(qh);
    }
}

__attribute__((unused)) static void ehci_qh_set_endpoint(ehci_qh_t *qh, uint8_t dev, uint8_t ep, uint16_t mps, bool in) {
    if (!qh) return;
    uint32_t ep_char = 0;
    ep_char |= dev & 0x7F;
    ep_char |= (ep & 0x0F) << 8;
    ep_char |= (in ? 1u : 0u) << 15;
    ep_char |= ((uint32_t)mps & 0x7FFu) << 16;
    ep_char |= (2u << 12); /* High-speed by default */
    qh->ep_char = ep_char;
}
