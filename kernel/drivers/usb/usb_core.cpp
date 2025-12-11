/**
 * Simple host-side USB stub used by unit tests.
 */

#include <drivers/usb/usb_core.hpp>
#include <drivers/usb/usb_transfer.h>
#include <drivers/usb/usb_device.h>
#include <drivers/usb/usb_descriptors.h>
#include "klog.h"
#include "string.h"

namespace usb {

static usb_controller_t g_controllers[4];
static uint32_t g_controller_count = 0;
static usb_device_t *g_devices[4];
static uint32_t g_device_count = 0;
static usb_device_descriptor_t g_device_desc{};

static void setup_stub_controllers() {
    if (g_controller_count > 0) {
        return;
    }
    /* Order matters for tests */
    g_controllers[0].type = USB_CONTROLLER_XHCI;
    g_controllers[0].name = "xhci";
    usb_host_register(&g_controllers[0]);

    g_controllers[1].type = USB_CONTROLLER_EHCI;
    g_controllers[1].name = "ehci";
    usb_host_register(&g_controllers[1]);

    g_controllers[2].type = USB_CONTROLLER_OHCI;
    g_controllers[2].name = "ohci";
    usb_host_register(&g_controllers[2]);

    g_controller_count = 3;
}

static void setup_stub_device() {
    if (g_device_count > 0) {
        return;
    }

    usb_device_t *dev = usb_device_alloc();
    if (!dev) {
        return;
    }
    dev->controller = &g_controllers[0];
    dev->vendor_id = 0x1234;
    dev->product_id = 0x5678;
    dev->device_class = 0x03;     /* HID */
    dev->device_subclass = 0x01;  /* Boot interface */
    dev->device_protocol = 0x01;  /* Keyboard */
    dev->speed = USB_SPEED_FULL;
    dev->num_configurations = 1;
    dev->state = USB_DEVICE_STATE_CONFIGURED;
    usb_device_set_address(dev, 1);
    usb_device_set_configuration(dev, 1);
    usb_device_add_endpoint(dev, 0x81, USB_ENDPOINT_XFER_INT, 8, 1);

    /* Populate device descriptor */
    g_device_desc.bLength = sizeof(usb_device_descriptor_t);
    g_device_desc.bDescriptorType = USB_DT_DEVICE;
    g_device_desc.bcdUSB = USB_VERSION_2_0;
    g_device_desc.bDeviceClass = dev->device_class;
    g_device_desc.bDeviceSubClass = dev->device_subclass;
    g_device_desc.bDeviceProtocol = dev->device_protocol;
    g_device_desc.bMaxPacketSize0 = 8;
    g_device_desc.idVendor = dev->vendor_id;
    g_device_desc.idProduct = dev->product_id;
    g_device_desc.bcdDevice = 0x0100;
    g_device_desc.iManufacturer = 1;
    g_device_desc.iProduct = 2;
    g_device_desc.iSerialNumber = 3;
    g_device_desc.bNumConfigurations = dev->num_configurations;

    dev->descriptors = &g_device_desc;
    dev->descriptors_size = sizeof(g_device_desc);

    usb_device_list_add(dev);
    g_devices[g_device_count++] = dev;
}

uint32_t controller_count() {
    return g_controller_count;
}

uint32_t device_count() {
    return g_device_count;
}

int usb_init() {
    usb_core_init();
    setup_stub_controllers();
    setup_stub_device();
    return 0;
}

void usb_poll() {
    /* Nothing to poll in stubbed host mode */
}

const usb_controller_t *controller_at(int idx) {
    if (idx < 0 || static_cast<uint32_t>(idx) >= g_controller_count) {
        return nullptr;
    }
    return &g_controllers[idx];
}

const usb_device_t *device_at(int idx) {
    if (idx < 0 || static_cast<uint32_t>(idx) >= g_device_count) {
        return nullptr;
    }
    return g_devices[idx];
}

const usb_device_t *usb_stack_device_at(int idx) {
    return device_at(idx);
}

const usb_device_descriptor_t *usb_device_get_descriptor(const usb_device_t *dev) {
    if (!dev) {
        return nullptr;
    }
    return static_cast<const usb_device_descriptor_t *>(dev->descriptors);
}

}  // namespace usb

extern "C" bool usb_device_has_hid_keyboard(const usb_device_t *dev) {
    return dev && dev->device_class == 0x03 && dev->device_subclass == 0x01;
}

extern "C" uint8_t usb_device_ep_in_addr(const usb_device_t *dev) {
    if (!dev) {
        return 0;
    }
    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        uint8_t addr = dev->endpoints[i].address;
        if (addr & USB_ENDPOINT_DIR_IN) {
            return addr;
        }
    }
    return 0;
}

extern "C" uint16_t usb_device_ep_in_max_packet(const usb_device_t *dev) {
    if (!dev) {
        return 0;
    }
    for (uint8_t i = 0; i < dev->num_endpoints; i++) {
        uint8_t addr = dev->endpoints[i].address;
        if (addr & USB_ENDPOINT_DIR_IN) {
            return dev->endpoints[i].max_packet_size;
        }
    }
    return 0;
}
