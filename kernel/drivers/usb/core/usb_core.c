#include "../include/usb_core.h"
#include "../include/usb_defs.h"
#include "../include/usb_controller.h"
#include "../include/xhci.h"
#include "../include/usb_request.h"
#include "../include/usb_transfer.h"
#include "../include/usb_device.h"

extern void* memset(void* dest, int val, size_t n);

/* Simple global USB state seeded with placeholder controllers. */
static usb_controller_t g_controllers[3];
static xhci_controller_t g_xhci;
static usb_stack_state_t g_state;
static usb_device_t g_devices[32];

#ifdef __cplusplus
extern "C" {
#endif

static void seed_controllers(void) {
    g_controllers[0].type = USB_CONTROLLER_XHCI;
    g_controllers[0].bus_number = 0;
    g_controllers[0].slot_number = 0;
    g_controllers[0].max_speed = USB_SPEED_SUPER;
    g_controllers[0].driver_data = 0;

    g_controllers[1].type = USB_CONTROLLER_EHCI;
    g_controllers[1].bus_number = 0;
    g_controllers[1].slot_number = 1;
    g_controllers[1].max_speed = USB_SPEED_HIGH;
    g_controllers[1].driver_data = 0;

    g_controllers[2].type = USB_CONTROLLER_OHCI;
    g_controllers[2].bus_number = 0;
    g_controllers[2].slot_number = 2;
    g_controllers[2].max_speed = USB_SPEED_FULL;
    g_controllers[2].driver_data = 0;
}

void usb_stack_init(void) {
    g_state.controller_count = 3;
    g_state.device_count = 0;
    seed_controllers();
    /* Link xHCI extended state to first controller. */
    g_controllers[0].driver_data = &g_xhci;
    g_xhci.base = g_controllers[0];
    for (uint32_t i = 0; i < g_state.controller_count; ++i) {
        g_state.controllers[i] = &g_controllers[i];
    }
    for (uint32_t i = g_state.controller_count; i < 8; ++i) {
        g_state.controllers[i] = 0;
    }
    for (uint32_t i = 0; i < 32; ++i) {
        g_state.devices[i] = 0;
        memset(&g_devices[i], 0, sizeof(usb_device_t));
    }
}

void usb_stack_poll(void) {
    /* Poll xHCI event ring if present. */
    if (g_state.controller_count > 0 && g_controllers[0].driver_data) {
        xhci_controller_t* xhci = (xhci_controller_t*)g_controllers[0].driver_data;
        xhci_reap_and_arm(xhci);
    }
}

const usb_stack_state_t *usb_stack_state(void) {
    return &g_state;
}

uint32_t usb_stack_controller_count(void) {
    return g_state.controller_count;
}

uint32_t usb_stack_device_count(void) {
    return g_state.device_count;
}

const usb_controller_t *usb_stack_controller_at(uint32_t index) {
    if (index >= g_state.controller_count) {
        return 0;
    }
    return g_state.controllers[index];
}

bool usb_stack_add_device(usb_controller_t* ctrl, usb_speed_t speed) {
    if (!ctrl || g_state.device_count >= 32) {
        return false;
    }
    usb_device_t* dev = &g_devices[g_state.device_count];
    dev->controller = ctrl;
    dev->speed = speed;
    dev->address = (uint8_t)(g_state.device_count + 1);
    dev->port = (uint8_t)g_state.device_count;
    dev->configuration_value = 1;
    dev->interface_count = 1;
    g_state.devices[g_state.device_count] = dev;
    g_state.device_count++;
    return true;
}

const usb_device_t* usb_stack_device_at(uint32_t index) {
    if (index >= g_state.device_count) {
        return 0;
    }
    return g_state.devices[index];
}

bool usb_stack_enumerate_basic() {
    /* Simulate a single device enumeration on xHCI slot 1. */
    usb_controller_t* ctrl = g_state.controllers[0];
    if (!ctrl || ctrl->type != USB_CONTROLLER_XHCI) {
        return false;
    }
    usb_device_t* dev = &g_devices[g_state.device_count];
    dev->controller = ctrl;
    dev->speed = USB_SPEED_SUPER;
    dev->address = 1;
    dev->configuration_value = 1;
    dev->interface_count = 1;

    usb_setup_packet_t setup;
    usb_device_descriptor_t desc;
    usb_transfer_t xfer;
    memset(&setup, 0, sizeof(setup));
    memset(&desc, 0, sizeof(desc));
    memset(&xfer, 0, sizeof(xfer));
    usb_req_build_get_descriptor(&setup, USB_DT_DEVICE, 0, sizeof(usb_device_descriptor_t));
    xfer.buffer = &desc;
    xfer.length = sizeof(desc);
    usb_submit_control(&xfer, &setup);
    usb_device_set_descriptor(dev, &desc);

    usb_req_build_set_address(&setup, dev->address);
    usb_submit_control(&xfer, &setup);

    usb_req_build_set_configuration(&setup, dev->configuration_value);
    usb_submit_control(&xfer, &setup);

    g_state.devices[g_state.device_count] = dev;
    g_state.device_count++;
    return true;
}

#ifdef __cplusplus
}
#endif
