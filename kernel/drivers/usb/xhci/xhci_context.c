/**
 * xHCI context helpers.
 */

#include <drivers/usb/xhci.h>
#include "klog.h"
#include "string.h"

__attribute__((unused)) static void xhci_init_device_context(xhci_device_context_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

__attribute__((unused)) static void xhci_init_input_context(xhci_input_context_t *ictx) {
    if (!ictx) return;
    memset(ictx, 0, sizeof(*ictx));
    ictx->add_context_flags = 1; /* slot context present */
}
