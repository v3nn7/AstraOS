#pragma once

#include "types.h"
#include "klog.h"

/**
 * USB Logging Utilities
 * 
 * Provides convenient macros for USB-related logging.
 */

#define USB_LOG_ERROR(fmt, ...) \
    klog_printf(KLOG_ERROR, "[USB] " fmt, ##__VA_ARGS__)

#define USB_LOG_WARN(fmt, ...) \
    klog_printf(KLOG_WARN, "[USB] " fmt, ##__VA_ARGS__)

#define USB_LOG_INFO(fmt, ...) \
    klog_printf(KLOG_INFO, "[USB] " fmt, ##__VA_ARGS__)

#define USB_LOG_DEBUG(fmt, ...) \
    klog_printf(KLOG_DEBUG, "[USB] " fmt, ##__VA_ARGS__)

/* Controller-specific logging */
#define USB_LOG_XHCI(fmt, ...) \
    klog_printf(KLOG_INFO, "[XHCI] " fmt, ##__VA_ARGS__)

#define USB_LOG_EHCI(fmt, ...) \
    klog_printf(KLOG_INFO, "[EHCI] " fmt, ##__VA_ARGS__)

#define USB_LOG_OHCI(fmt, ...) \
    klog_printf(KLOG_INFO, "[OHCI] " fmt, ##__VA_ARGS__)

#define USB_LOG_UHCI(fmt, ...) \
    klog_printf(KLOG_INFO, "[UHCI] " fmt, ##__VA_ARGS__)

#define USB_LOG_HID(fmt, ...) \
    klog_printf(KLOG_INFO, "[HID] " fmt, ##__VA_ARGS__)

