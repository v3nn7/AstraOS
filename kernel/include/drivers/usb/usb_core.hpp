#pragma once

#include <drivers/usb/usb_core.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_descriptors.h>

namespace usb {

using usb_controller_t = usb_host_controller_t;

uint32_t controller_count();
uint32_t device_count();
int usb_init();
void usb_poll();
const usb_controller_t* controller_at(int idx);
const usb_device_t* device_at(int idx);
const usb_device_t* usb_stack_device_at(int idx);
const usb_device_descriptor_t* usb_device_get_descriptor(const usb_device_t* dev);

}  // namespace usb

/* Global helpers used by host tests */
extern "C" bool usb_device_has_hid_keyboard(const usb_device_t* dev);
extern "C" uint8_t usb_device_ep_in_addr(const usb_device_t* dev);
extern "C" uint16_t usb_device_ep_in_max_packet(const usb_device_t* dev);
