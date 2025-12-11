/**
 * xHCI capability helpers.
 */

#include <drivers/usb/xhci.h>
#include "mmio.h"
#include "klog.h"

__attribute__((unused)) static void xhci_dump_caps(uintptr_t base) {
    uint32_t caplen = mmio_read8((volatile uint8_t *)(base + XHCI_CAPLENGTH));
    uint32_t hci_ver = mmio_read32((volatile uint32_t *)(base + XHCI_HCIVERSION)) & 0xFFFFu;
    uint32_t hcs1 = mmio_read32((volatile uint32_t *)(base + XHCI_HCSPARAMS1));
    klog_printf(KLOG_INFO, "xhci: caplen=%u hci_ver=0x%04x hcs1=0x%08x", caplen, (unsigned)hci_ver, hcs1);
}
