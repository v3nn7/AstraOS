/**
 * xHCI simplistic scheduler helpers.
 */

#include <drivers/usb/xhci.h>
#include "klog.h"

__attribute__((unused)) static void xhci_schedule_doorbell(xhci_controller_t *ctrl, uint8_t slot, uint8_t ep) {
    if (!ctrl || !ctrl->doorbell_regs) return;
    volatile uint32_t *db = (volatile uint32_t *)ctrl->doorbell_regs;
    db[slot] = ep;
    klog_printf(KLOG_INFO, "xhci: ring doorbell slot=%u ep=%u", slot, ep);
}
