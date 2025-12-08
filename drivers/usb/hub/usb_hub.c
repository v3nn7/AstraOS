/**
 * USB Hub Driver
 * 
 * Basic USB hub support for port management and device discovery.
 * Required for laptops where USB devices are connected through hubs.
 */

#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "usb/usb_descriptors.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

/* USB Hub Class */
#define USB_CLASS_HUB 0x09

/* Hub Descriptor */
typedef struct PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t bPwrOn2PwrGood;
    uint8_t bHubContrCurrent;
    uint8_t DeviceRemovable[64]; /* Variable length */
    uint8_t PortPwrCtrlMask[64]; /* Variable length */
} usb_hub_descriptor_t;

/* Hub Request Codes */
#define USB_HUB_REQ_GET_STATUS      0x00
#define USB_HUB_REQ_CLEAR_FEATURE   0x01
#define USB_HUB_REQ_SET_FEATURE     0x03
#define USB_HUB_REQ_GET_DESCRIPTOR  0x06
#define USB_HUB_REQ_SET_DESCRIPTOR  0x07

/* Hub Features */
#define USB_HUB_FEATURE_PORT_RESET  0x04
#define USB_HUB_FEATURE_PORT_POWER  0x08
#define USB_HUB_FEATURE_C_PORT_CONNECTION 0x10
#define USB_HUB_FEATURE_C_PORT_RESET 0x14
#define USB_HUB_FEATURE_C_PORT_ENABLE 0x16

/* Hub Port Status */
typedef struct PACKED {
    uint16_t port_status;
    uint16_t port_change;
} usb_hub_port_status_t;

/* Hub Device Data */
typedef struct {
    usb_device_t *device;
    uint8_t num_ports;
    usb_hub_descriptor_t descriptor;
} usb_hub_device_t;

/**
 * Probe device for hub support
 */
int usb_hub_probe(usb_device_t *dev) {
    if (!dev) return -1;
    
    /* Check if device is a hub */
    if (dev->device_class == USB_CLASS_HUB) {
        klog_printf(KLOG_INFO, "usb_hub: found hub device VID:PID=%04x:%04x",
                    dev->vendor_id, dev->product_id);
        return 0;
    }
    
    return -1;
}

/**
 * Initialize hub
 */
int usb_hub_init(usb_device_t *dev) {
    if (!dev) return -1;
    
    usb_hub_device_t *hub = (usb_hub_device_t *)kmalloc(sizeof(usb_hub_device_t));
    if (!hub) {
        return -1;
    }
    
    k_memset(hub, 0, sizeof(usb_hub_device_t));
    hub->device = dev;
    dev->driver_data = hub;
    
    /* Get hub descriptor */
    uint8_t buffer[64];
    int ret = usb_control_transfer(dev,
                                   USB_REQ_TYPE_CLASS | USB_REQ_TYPE_DEVICE | USB_ENDPOINT_DIR_IN,
                                   USB_HUB_REQ_GET_DESCRIPTOR,
                                   (USB_DT_HUB << 8) | 0,
                                   0,
                                   buffer,
                                   64,
                                   1000);
    
    if (ret >= 0 && ret >= 7) {
        k_memcpy(&hub->descriptor, buffer, sizeof(usb_hub_descriptor_t));
        hub->num_ports = hub->descriptor.bNbrPorts;
        klog_printf(KLOG_INFO, "usb_hub: hub has %u ports", hub->num_ports);
    } else {
        klog_printf(KLOG_WARN, "usb_hub: failed to get hub descriptor");
        hub->num_ports = 0;
    }
    
    /* Scan ports after initialization */
    extern int usb_hub_scan_ports(usb_device_t *hub_dev);
    usb_hub_scan_ports(dev);
    
    return 0;
}

/**
 * Get hub port status
 */
int usb_hub_get_port_status(usb_device_t *hub_dev, uint8_t port, usb_hub_port_status_t *status) {
    if (!hub_dev || !status || port == 0) return -1;
    
    usb_hub_device_t *hub = (usb_hub_device_t *)hub_dev->driver_data;
    if (!hub || port > hub->num_ports) return -1;
    
    uint16_t status_data[2];
    int ret = usb_control_transfer(hub_dev,
                                   USB_REQ_TYPE_CLASS | USB_REQ_TYPE_OTHER | USB_ENDPOINT_DIR_IN,
                                   USB_HUB_REQ_GET_STATUS,
                                   0,
                                   port,
                                   status_data,
                                   4,
                                   1000);
    
    if (ret >= 4) {
        status->port_status = status_data[0];
        status->port_change = status_data[1];
        return 0;
    }
    
    return -1;
}

/**
 * Reset hub port
 */
int usb_hub_reset_port(usb_device_t *hub_dev, uint8_t port) {
    if (!hub_dev || port == 0) return -1;
    
    /* Set port reset feature */
    int ret = usb_control_transfer(hub_dev,
                                   USB_REQ_TYPE_CLASS | USB_REQ_TYPE_OTHER | USB_ENDPOINT_DIR_OUT,
                                   USB_HUB_REQ_SET_FEATURE,
                                   USB_HUB_FEATURE_PORT_RESET,
                                   port,
                                   NULL,
                                   0,
                                   1000);
    
    if (ret < 0) {
        return -1;
    }
    
    /* Wait for reset to complete */
    uint32_t timeout = 10000; /* 10 seconds */
    while (timeout-- > 0) {
        usb_hub_port_status_t status;
        if (usb_hub_get_port_status(hub_dev, port, &status) == 0) {
            if (!(status.port_status & (1 << 4))) { /* Port Reset bit cleared */
                break;
            }
        }
        for (volatile int i = 0; i < 1000; i++);
    }
    
    /* Clear port reset change */
    usb_control_transfer(hub_dev,
                        USB_REQ_TYPE_CLASS | USB_REQ_TYPE_OTHER | USB_ENDPOINT_DIR_OUT,
                        USB_HUB_REQ_CLEAR_FEATURE,
                        USB_HUB_FEATURE_C_PORT_RESET,
                        port,
                        NULL,
                        0,
                        1000);
    
    klog_printf(KLOG_INFO, "usb_hub: port %u reset", port);
    return 0;
}

/**
 * Enable hub port power
 */
int usb_hub_port_power_on(usb_device_t *hub_dev, uint8_t port) {
    if (!hub_dev || port == 0) return -1;
    
    return usb_control_transfer(hub_dev,
                               USB_REQ_TYPE_CLASS | USB_REQ_TYPE_OTHER | USB_ENDPOINT_DIR_OUT,
                               USB_HUB_REQ_SET_FEATURE,
                               USB_HUB_FEATURE_PORT_POWER,
                               port,
                               NULL,
                               0,
                               1000);
}

/**
 * Scan hub ports for devices
 */
int usb_hub_scan_ports(usb_device_t *hub_dev) {
    if (!hub_dev) return -1;
    
    usb_hub_device_t *hub = (usb_hub_device_t *)hub_dev->driver_data;
    if (!hub) return -1;
    
    klog_printf(KLOG_INFO, "usb_hub: scanning %u ports", hub->num_ports);
    
    for (uint8_t port = 1; port <= hub->num_ports; port++) {
        usb_hub_port_status_t status;
        if (usb_hub_get_port_status(hub_dev, port, &status) == 0) {
            if (status.port_status & (1 << 0)) { /* Port Connected */
                klog_printf(KLOG_INFO, "usb_hub: port %u has device connected", port);
                
                /* Reset port */
                if (usb_hub_reset_port(hub_dev, port) == 0) {
                    klog_printf(KLOG_INFO, "usb_hub: port %u reset, enumerating device...", port);
                    
                    /* Wait a bit for port to stabilize */
                    for (volatile int i = 0; i < 10000; i++);
                    
                    /* Check port status again after reset */
                    usb_hub_port_status_t status_after_reset;
                    if (usb_hub_get_port_status(hub_dev, port, &status_after_reset) == 0) {
                        if (status_after_reset.port_status & (1 << 1)) { /* Port Enabled */
                            /* Create device for enumeration */
                            extern usb_device_t *usb_device_alloc(void);
                            usb_device_t *dev = usb_device_alloc();
                            if (dev) {
                                dev->controller = hub_dev->controller;
                                dev->parent = hub_dev;
                                dev->port = port;
                                dev->address = 0; /* Will be set during enumeration */
                                
                                /* Enumerate device */
                                if (usb_device_enumerate(dev) == 0) {
                                    klog_printf(KLOG_INFO, "usb_hub: device enumerated successfully on port %u (address=%d, VID:PID=%04x:%04x)", 
                                               port, dev->address, dev->vendor_id, dev->product_id);
                                    
                                    /* usb_device_enumerate already calls usb_bind_driver() */
                                } else {
                                    klog_printf(KLOG_WARN, "usb_hub: failed to enumerate device on port %u", port);
                                    extern void usb_device_free(usb_device_t *dev);
                                    usb_device_free(dev);
                                }
                            } else {
                                klog_printf(KLOG_WARN, "usb_hub: failed to allocate device structure for port %u", port);
                            }
                        } else {
                            klog_printf(KLOG_WARN, "usb_hub: port %u not enabled after reset", port);
                        }
                    } else {
                        klog_printf(KLOG_WARN, "usb_hub: failed to get port status after reset for port %u", port);
                    }
                } else {
                    klog_printf(KLOG_WARN, "usb_hub: failed to reset port %u", port);
                }
            }
        }
    }
    
    return 0;
}

/**
 * Register hub driver
 */
void usb_hub_register_driver(void) {
    extern int usb_register_driver(usb_driver_t *drv);
    
    static usb_driver_t usb_hub_driver = {
        .name = "usb-hub",
        .probe = usb_hub_probe,
        .init = usb_hub_init,
        .remove = NULL
    };
    
    usb_register_driver(&usb_hub_driver);
    klog_printf(KLOG_INFO, "usb_hub: registered hub driver");
}
