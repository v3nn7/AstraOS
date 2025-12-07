/**
 * USB Helper Functions
 * 
 * Common utility functions used across USB stack.
 */

#include "usb/util/usb_helpers.h"
#include "usb/util/usb_log.h"
#include "usb/util/usb_bits.h"
#include "kmalloc.h"
#include "kernel.h"
#include "string.h"

/**
 * Calculate USB CRC5 checksum
 */
uint8_t usb_crc5(uint16_t data, uint8_t nbits) {
    uint8_t crc = 0x1F;
    uint8_t poly = 0x05;
    
    for (uint8_t i = 0; i < nbits; i++) {
        uint8_t bit = (data >> i) & 1;
        uint8_t crc_bit = crc & 1;
        
        if (bit ^ crc_bit) {
            crc = (crc >> 1) ^ poly;
        } else {
            crc = crc >> 1;
        }
    }
    
    return crc ^ 0x1F;
}

/**
 * Calculate USB CRC16 checksum
 */
uint16_t usb_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    uint16_t poly = 0x8005;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * Convert USB speed enum to string
 */
const char *usb_speed_string(usb_speed_t speed) {
    switch (speed) {
        case USB_SPEED_UNKNOWN:    return "Unknown";
        case USB_SPEED_LOW:        return "Low (1.5 Mbps)";
        case USB_SPEED_FULL:       return "Full (12 Mbps)";
        case USB_SPEED_HIGH:       return "High (480 Mbps)";
        case USB_SPEED_SUPER:      return "Super (5 Gbps)";
        case USB_SPEED_SUPER_PLUS: return "Super+ (10 Gbps)";
        default:                   return "Invalid";
    }
}

/**
 * Convert USB controller type to string
 */
const char *usb_controller_type_string(usb_controller_type_t type) {
    switch (type) {
        case USB_CONTROLLER_UHCI: return "UHCI";
        case USB_CONTROLLER_OHCI: return "OHCI";
        case USB_CONTROLLER_EHCI: return "EHCI";
        case USB_CONTROLLER_XHCI: return "XHCI";
        default:                  return "Unknown";
    }
}

/**
 * Convert USB endpoint type to string
 */
const char *usb_endpoint_type_string(uint8_t type) {
    switch (type) {
        case USB_ENDPOINT_XFER_CONTROL: return "Control";
        case USB_ENDPOINT_XFER_ISOC:    return "Isochronous";
        case USB_ENDPOINT_XFER_BULK:    return "Bulk";
        case USB_ENDPOINT_XFER_INT:     return "Interrupt";
        default:                        return "Unknown";
    }
}

/**
 * Extract endpoint address from endpoint descriptor
 */
uint8_t usb_endpoint_address(uint8_t bEndpointAddress) {
    return bEndpointAddress & 0x7F;
}

/**
 * Extract endpoint direction from endpoint descriptor
 */
uint8_t usb_endpoint_direction(uint8_t bEndpointAddress) {
    return (bEndpointAddress >> 7) & 1;
}

/**
 * Extract endpoint type from endpoint attributes
 */
uint8_t usb_endpoint_type(uint8_t bmAttributes) {
    return bmAttributes & 0x03;
}

/**
 * Extract endpoint synchronization type from attributes
 */
uint8_t usb_endpoint_sync_type(uint8_t bmAttributes) {
    return (bmAttributes >> 2) & 0x03;
}

/**
 * Extract endpoint usage type from attributes
 */
uint8_t usb_endpoint_usage_type(uint8_t bmAttributes) {
    return (bmAttributes >> 4) & 0x03;
}

/**
 * Calculate USB frame number from microframes
 */
uint16_t usb_frame_from_microframe(uint32_t microframe) {
    return (uint16_t)(microframe / 8);
}

/**
 * Calculate USB microframe from frame
 */
uint8_t usb_microframe_from_frame(uint16_t frame) {
    return (uint8_t)(frame % 8);
}

/**
 * Wait for a condition with timeout
 */
int usb_wait_for_condition(bool (*condition)(void), uint32_t timeout_us) {
    uint32_t start = 0; /* TODO: Get current time in microseconds */
    uint32_t elapsed = 0;
    
    while (elapsed < timeout_us) {
        if (condition()) {
            return 0;
        }
        /* TODO: Sleep for a short time */
        elapsed = 0; /* TODO: Calculate elapsed time */
    }
    
    return -1; /* Timeout */
}

/**
 * Dump USB descriptor in hex format
 */
void usb_dump_descriptor(const uint8_t *data, size_t length) {
    USB_LOG_DEBUG("Descriptor dump (%zu bytes):", length);
    for (size_t i = 0; i < length; i += 16) {
        printf("  %04zx: ", i);
        for (size_t j = 0; j < 16 && (i + j) < length; j++) {
            printf("%02x ", data[i + j]);
        }
        printf("\n");
    }
}

