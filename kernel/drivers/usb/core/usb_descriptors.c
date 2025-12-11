/**
 * USB Descriptor helpers - stubbed parsing and retrieval
 */

#include <drivers/usb/usb_descriptors.h>
#include <drivers/usb/usb_device.h>
#include "string.h"

int usb_parse_device_descriptor(const uint8_t *data, size_t length, usb_device_descriptor_t *desc) {
    if (!data || !desc || length < sizeof(usb_device_descriptor_t)) {
        return -1;
    }
    memcpy(desc, data, sizeof(usb_device_descriptor_t));
    return 0;
}

int usb_parse_configuration_descriptor(const uint8_t *data, size_t length,
                                       usb_configuration_descriptor_t *config) {
    if (!data || !config || length < sizeof(usb_configuration_descriptor_t)) {
        return -1;
    }
    memcpy(config, data, sizeof(usb_configuration_descriptor_t));
    return 0;
}

int usb_parse_interface_descriptor(const uint8_t *data, size_t length,
                                   usb_interface_descriptor_t *iface) {
    if (!data || !iface || length < sizeof(usb_interface_descriptor_t)) {
        return -1;
    }
    memcpy(iface, data, sizeof(usb_interface_descriptor_t));
    return 0;
}

int usb_parse_endpoint_descriptor(const uint8_t *data, size_t length,
                                  usb_endpoint_descriptor_t *ep) {
    if (!data || !ep || length < sizeof(usb_endpoint_descriptor_t)) {
        return -1;
    }
    memcpy(ep, data, sizeof(usb_endpoint_descriptor_t));
    return 0;
}

int usb_parse_hid_descriptor(const uint8_t *data, size_t length, usb_hid_descriptor_t *hid) {
    if (!data || !hid || length < sizeof(usb_hid_descriptor_t)) {
        return -1;
    }
    memcpy(hid, data, sizeof(usb_hid_descriptor_t));
    return 0;
}

int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       uint16_t lang_id, void *buffer, size_t length) {
    (void)index;
    (void)lang_id;
    if (!dev || !buffer) {
        return -1;
    }

    if (type == USB_DT_DEVICE && dev->descriptors && dev->descriptors_size >= length) {
        memcpy(buffer, dev->descriptors, length);
        return (int)length;
    }

    /* For other descriptor types in stubbed mode, just zero the buffer */
    memset(buffer, 0, length);
    return (int)length;
}

int usb_get_string_descriptor(usb_device_t *dev, uint8_t index, uint16_t lang_id,
                              char *buffer, size_t length) {
    (void)dev;
    (void)index;
    (void)lang_id;
    if (!buffer || length == 0) {
        return -1;
    }
    buffer[0] = '\0';
    return 0;
}

int usb_device_get_configurations(usb_device_t *dev) {
    /* Stub: nothing to enumerate */
    return dev ? 0 : -1;
}

int usb_parse_configuration(usb_device_t *dev, uint8_t config_index,
                            const uint8_t *data, size_t length) {
    (void)dev;
    (void)config_index;
    (void)data;
    (void)length;
    return 0;
}
