/**
 * USB memory helpers (DMA-safe wrappers around kmalloc/kmemalign).
 */

#include "kmalloc.h"
#include "klog.h"
#include <stddef.h>

static void *usb_mem_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    void *p = kmalloc(size);
    if (!p) {
        klog_printf(KLOG_ERROR, "usb: alloc %zu bytes failed", (unsigned long)size);
    }
    return p;
}

static void *usb_mem_alloc_aligned(size_t alignment, size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (alignment == 0) {
        alignment = 16;
    }
    void *p = kmemalign(alignment, size);
    if (!p) {
        klog_printf(KLOG_ERROR, "usb: alloc align=%zu size=%zu failed",
                    (unsigned long)alignment, (unsigned long)size);
    }
    return p;
}

static void usb_mem_free(void *ptr) {
    if (ptr) {
        kfree(ptr);
    }
}
