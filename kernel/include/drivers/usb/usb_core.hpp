#pragma once

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_descriptors.h>

namespace usb {

static inline uint32_t controller_count() { return 0; }
static inline uint32_t device_count() { return 0; }
static inline int usb_init() { return 0; }
static inline void usb_poll() {}
static inline const usb_device_t* device_at(int) { return nullptr; }
static inline const usb_device_t* usb_stack_device_at(int idx) { return device_at(idx); }
static inline const usb_device_descriptor_t* usb_device_get_descriptor(const usb_device_t*) { return nullptr; }

}  // namespace usb
