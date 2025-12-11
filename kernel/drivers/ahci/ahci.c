/**
 * Minimal AHCI stub: initialize controller and provide read_lba that returns zeroed data.
 * This keeps the kernel booting and callers unblocked until full AHCI is implemented.
 */

#include "ahci.h"
#include "klog.h"
#include "dma.h"
#include <string.h>

int ahci_init(void) {
    klog_printf(KLOG_INFO, "ahci: init (stub)");
    return 0;
}

int ahci_read_lba(uint64_t lba, void *buf, size_t sectors) {
    if (!buf || sectors == 0) return -1;
    /* Stub: zero-fill */
    memset(buf, 0, sectors * 512);
    klog_printf(KLOG_INFO, "ahci: read stub lba=%llu sectors=%zu", (unsigned long long)lba, sectors);
    return 0;
}

