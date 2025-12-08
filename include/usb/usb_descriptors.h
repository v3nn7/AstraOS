#pragma once

#include "types.h"

typedef struct usb_device usb_device_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB Descriptors Parser
 * 
 * Parses and manages USB device, configuration, interface, endpoint,
 * HID, and string descriptors.
 */

/* USB Descriptor Structures (packed) */
typedef struct PACKED usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_device_descriptor_t;

typedef struct PACKED usb_configuration_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} usb_configuration_descriptor_t;

typedef struct PACKED usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

typedef struct PACKED usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usb_endpoint_descriptor_t;

typedef struct PACKED usb_hid_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bDescriptorType2;
    uint16_t wDescriptorLength;
} usb_hid_descriptor_t;

typedef struct PACKED usb_string_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[];
} usb_string_descriptor_t;

/* Descriptor parsing functions */
int usb_parse_device_descriptor(const uint8_t *data, size_t length, usb_device_descriptor_t *desc);
int usb_parse_configuration_descriptor(const uint8_t *data, size_t length,
                                       usb_configuration_descriptor_t *config);
int usb_parse_interface_descriptor(const uint8_t *data, size_t length,
                                    usb_interface_descriptor_t *iface);
int usb_parse_endpoint_descriptor(const uint8_t *data, size_t length,
                                   usb_endpoint_descriptor_t *ep);
int usb_parse_hid_descriptor(const uint8_t *data, size_t length, usb_hid_descriptor_t *hid);

/* Get descriptor from device */
int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       uint16_t lang_id, void *buffer, size_t length);
int usb_get_string_descriptor(usb_device_t *dev, uint8_t index, uint16_t lang_id,
                              char *buffer, size_t length);

/* Get all configurations */
int usb_device_get_configurations(usb_device_t *dev);

/* Forward declaration */
struct usb_device;

/* Parse full configuration */
int usb_parse_configuration(usb_device_t *dev, uint8_t config_index,
                            const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

