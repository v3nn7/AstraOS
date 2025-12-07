/**
 * USB Core Implementation
 * 
 * Manages USB device tree, enumeration, and provides unified API
 * for all USB host controllers.
 */

#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "usb/usb_descriptors.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* Functions moved to usb_manager.c to avoid duplicate definitions */

