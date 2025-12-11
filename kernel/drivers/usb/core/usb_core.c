/**
 * USB Core Implementation
 * 
 * Manages USB device tree, enumeration, and provides unified API
 * for all USB host controllers.
 */

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_transfer.h>
#include <drivers/usb/usb_descriptors.h>
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* Functions moved to usb_manager.c to avoid duplicate definitions */

