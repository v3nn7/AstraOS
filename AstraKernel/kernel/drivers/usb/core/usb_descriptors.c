/**
 * USB Descriptors Implementation
 * 
 * Parses USB descriptors and extracts device information.
 */

#include "usb/usb_descriptors.h"
#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/**
 * Parse device descriptor
 */
int usb_parse_device_descriptor(const uint8_t *data, size_t length, usb_device_descriptor_t *desc) {
    if (!data || !desc || length < 18) {
        return -1;
    }

    memcpy(desc, data, sizeof(usb_device_descriptor_t));
    if (desc->bLength != 18 || desc->bDescriptorType != USB_DT_DEVICE) {
        return -1;
    }

    return 0;
}

/**
 * Parse configuration descriptor
 */
int usb_parse_configuration_descriptor(const uint8_t *data, size_t length,
                                        usb_configuration_descriptor_t *config) {
    if (!data || !config || length < 9) {
        return -1;
    }

    memcpy(config, data, sizeof(usb_configuration_descriptor_t));
    if (config->bLength != 9 || config->bDescriptorType != USB_DT_CONFIGURATION) {
        return -1;
    }

    return 0;
}

/**
 * Parse interface descriptor
 */
int usb_parse_interface_descriptor(const uint8_t *data, size_t length,
                                    usb_interface_descriptor_t *iface) {
    if (!data || !iface || length < 9) {
        return -1;
    }

    memcpy(iface, data, sizeof(usb_interface_descriptor_t));
    if (iface->bLength != 9 || iface->bDescriptorType != USB_DT_INTERFACE) {
        return -1;
    }

    return 0;
}

/**
 * Parse endpoint descriptor
 */
int usb_parse_endpoint_descriptor(const uint8_t *data, size_t length,
                                   usb_endpoint_descriptor_t *ep) {
    if (!data || !ep || length < 7) {
        return -1;
    }

    memcpy(ep, data, sizeof(usb_endpoint_descriptor_t));
    if (ep->bLength < 7 || ep->bDescriptorType != USB_DT_ENDPOINT) {
        return -1;
    }

    return 0;
}

/**
 * Parse HID descriptor
 */
int usb_parse_hid_descriptor(const uint8_t *data, size_t length, usb_hid_descriptor_t *hid) {
    if (!data || !hid || length < 9) {
        return -1;
    }

    memcpy(hid, data, sizeof(usb_hid_descriptor_t));
    if (hid->bLength < 9 || hid->bDescriptorType != USB_DT_HID) {
        return -1;
    }

    return 0;
}

/**
 * Get descriptor from device
 */
int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       uint16_t lang_id, void *buffer, size_t length) {
    if (!dev || !buffer || length == 0) {
        return -1;
    }

    uint16_t wValue = (type << 8) | index;
    return usb_control_transfer(dev,
                                USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE | USB_ENDPOINT_DIR_IN,
                                USB_REQ_GET_DESCRIPTOR, wValue, lang_id, buffer, length, 1000);
}

/**
 * Get string descriptor
 */
int usb_get_string_descriptor(usb_device_t *dev, uint8_t index, uint16_t lang_id,
                              char *buffer, size_t length) {
    if (!dev || !buffer || length == 0) {
        return -1;
    }

    uint8_t *temp_buffer = (uint8_t *)kmalloc(256);
    if (!temp_buffer) {
        return -1;
    }

    int ret = usb_get_descriptor(dev, USB_DT_STRING, index, lang_id, temp_buffer, 256);
    if (ret < 0) {
        kfree(temp_buffer);
        return -1;
    }

    usb_string_descriptor_t *desc = (usb_string_descriptor_t *)temp_buffer;
    if (desc->bLength < 2 || desc->bDescriptorType != USB_DT_STRING) {
        kfree(temp_buffer);
        return -1;
    }

    /* Convert UTF-16LE to ASCII (simplified) */
    size_t str_len = (desc->bLength - 2) / 2;
    if (str_len > length - 1) {
        str_len = length - 1;
    }

    for (size_t i = 0; i < str_len; i++) {
        buffer[i] = (char)(desc->wString[i] & 0xFF);
    }
    buffer[str_len] = '\0';

    kfree(temp_buffer);
    return str_len;
}

/**
 * Get all configurations for a device
 */
int usb_device_get_configurations(usb_device_t *dev) {
    if (!dev) {
        return -1;
    }

    /* Get configuration descriptor for each configuration */
    for (uint8_t i = 0; i < dev->num_configurations; i++) {
        uint8_t config_buffer[9];
        int ret = usb_get_descriptor(dev, USB_DT_CONFIGURATION, i, 0, config_buffer, 9);
        if (ret < 0) {
            klog_printf(KLOG_WARN, "usb_descriptors: failed to get config %d", i);
            continue;
        }

        usb_configuration_descriptor_t config;
        if (usb_parse_configuration_descriptor(config_buffer, 9, &config) < 0) {
            continue;
        }

        /* Get full configuration descriptor */
        uint8_t *full_config = (uint8_t *)kmalloc(config.wTotalLength);
        if (!full_config) {
            continue;
        }

        ret = usb_get_descriptor(dev, USB_DT_CONFIGURATION, i, 0, full_config, config.wTotalLength);
        if (ret >= 0) {
            /* Parse full configuration */
            usb_parse_configuration(dev, i, full_config, config.wTotalLength);
        }

        kfree(full_config);
    }

    return 0;
}

/**
 * Parse full configuration descriptor
 */
int usb_parse_configuration(usb_device_t *dev, uint8_t config_index __attribute__((unused)),
                            const uint8_t *data, size_t length) {
    if (!dev || !data || length < 9) {
        return -1;
    }

    usb_configuration_descriptor_t config;
    if (usb_parse_configuration_descriptor(data, length, &config) < 0) {
        return -1;
    }

    size_t offset = config.bLength;
    uint8_t interface_num = 0xFF;

    /* Parse all descriptors in configuration */
    while (offset < length && offset < config.wTotalLength) {
        uint8_t desc_type = data[offset + 1];
        uint8_t desc_length = data[offset];

        if (desc_length == 0) {
            break;
        }

        switch (desc_type) {
            case USB_DT_INTERFACE: {
                usb_interface_descriptor_t iface;
                if (usb_parse_interface_descriptor(data + offset, desc_length, &iface) == 0) {
                    interface_num = iface.bInterfaceNumber;
                    klog_printf(KLOG_DEBUG, "usb_descriptors: interface %d class=%02x:%02x:%02x",
                                interface_num, iface.bInterfaceClass,
                                iface.bInterfaceSubClass, iface.bInterfaceProtocol);
                }
                break;
            }

            case USB_DT_ENDPOINT: {
                usb_endpoint_descriptor_t ep;
                if (usb_parse_endpoint_descriptor(data + offset, desc_length, &ep) == 0) {
                    uint8_t ep_type = ep.bmAttributes & 0x03;
                    uint16_t max_packet = ep.wMaxPacketSize & 0x7FF;
                    uint8_t interval = ep.bInterval;

                    usb_device_add_endpoint(dev, ep.bEndpointAddress, ep.bmAttributes,
                                           max_packet, interval);
                    klog_printf(KLOG_DEBUG, "usb_descriptors: endpoint 0x%02x type=%d max=%d",
                                ep.bEndpointAddress, ep_type, max_packet);
                }
                break;
            }

            case USB_DT_HID: {
                usb_hid_descriptor_t hid;
                if (usb_parse_hid_descriptor(data + offset, desc_length, &hid) == 0) {
                    klog_printf(KLOG_DEBUG, "usb_descriptors: HID descriptor report_len=%d",
                                hid.wDescriptorLength);
                }
                break;
            }
        }

        offset += desc_length;
    }

    return 0;
}

