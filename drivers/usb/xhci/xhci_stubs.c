#include "usb/xhci.h"
#include "usb/xhci_internal.h"
#include "klog.h"

void xhci_input_context_free(xhci_input_context_t *ctx) {
    (void)ctx;
}

void xhci_dump_trb_level(int level, const char *msg, xhci_trb_t *trb) {
    (void)level;
    (void)msg;
    (void)trb;
}

