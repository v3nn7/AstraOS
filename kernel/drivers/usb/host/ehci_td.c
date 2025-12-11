/**
 * EHCI Transfer Descriptor helpers (minimal).
 */

#include "kmalloc.h"
#include "klog.h"
#include <stdint.h>
#include <string.h>

typedef struct ehci_td {
    uint32_t next_td;
    uint32_t alt_next_td;
    uint32_t token;
    uint32_t buffer[5];
} ehci_td_t;

__attribute__((unused)) static ehci_td_t *ehci_td_alloc(void) {
    ehci_td_t *td = (ehci_td_t *)kmemalign(32, sizeof(ehci_td_t));
    if (!td) {
        klog_printf(KLOG_ERROR, "ehci: td alloc failed");
        return NULL;
    }
    memset(td, 0, sizeof(*td));
    td->next_td = 1;     /* T-bit set */
    td->alt_next_td = 1; /* T-bit set */
    return td;
}

__attribute__((unused)) static void ehci_td_free(ehci_td_t *td) {
    if (td) {
        kfree(td);
    }
}

__attribute__((unused)) static void ehci_td_set_buffer(ehci_td_t *td, uintptr_t phys, uint32_t len) {
    if (!td) return;
    td->buffer[0] = (uint32_t)(phys & ~0xFFFu);
    td->token = (len << 16) | (1u << 7); /* active */
}
