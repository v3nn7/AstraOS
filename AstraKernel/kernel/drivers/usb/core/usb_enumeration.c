/**
 * USB Enumeration
 * 
 * Handles device enumeration, configuration, and endpoint setup.
 */

#include "usb/usb_core.h"
#include "usb/usb_device.h"
#include "usb/usb_transfer.h"
#include "usb/usb_descriptors.h"
#include "usb/xhci.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"

extern void usb_device_list_add(usb_device_t *dev);
extern uint8_t usb_allocate_device_address(void);

/**
 * Get device descriptor helper
 */
static int usb_device_get_device_descriptor(usb_device_t *dev) {
    uint8_t buffer[18] __attribute__((aligned(64)));
    
    k_memset(buffer, 0, sizeof(buffer));
    
    int ret = usb_get_descriptor(dev, USB_DT_DEVICE, 0, 0, buffer, sizeof(buffer));
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get device descriptor (ret=%d)", ret);
        return -1;
    }
    
    klog_printf(KLOG_INFO, "usb_device: raw descriptor (%d bytes): %02x %02x %02x %02x %02x %02x %02x %02x",
                ret, buffer[0], buffer[1], buffer[2], buffer[3], 
                buffer[4], buffer[5], buffer[6], buffer[7]);
    klog_printf(KLOG_INFO, "usb_device: raw descriptor cont: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                buffer[8], buffer[9], buffer[10], buffer[11],
                buffer[12], buffer[13], buffer[14], buffer[15], buffer[16], buffer[17]);

    klog_printf(KLOG_INFO, "usb_device: VID bytes[8-9]=%02x %02x PID bytes[10-11]=%02x %02x",
                buffer[8], buffer[9], buffer[10], buffer[11]);

    uint16_t vid_le = (uint16_t)buffer[8] | ((uint16_t)buffer[9] << 8);
    uint16_t pid_le = (uint16_t)buffer[10] | ((uint16_t)buffer[11] << 8);
    klog_printf(KLOG_INFO, "usb_device: manual LE parse: VID=%04x PID=%04x", vid_le, pid_le);

    usb_device_descriptor_t *desc = (usb_device_descriptor_t *)buffer;
    
    klog_printf(KLOG_INFO, "usb_device: struct parse: VID=%04x PID=%04x class=%02x subclass=%02x protocol=%02x",
                desc->idVendor, desc->idProduct, desc->bDeviceClass, 
                desc->bDeviceSubClass, desc->bDeviceProtocol);
    
    if (desc->idVendor == 0x0000 && desc->idProduct == 0x0000) {
        if (vid_le != 0x0000 || pid_le != 0x0000) {
            klog_printf(KLOG_WARN, "usb_device: struct VID/PID=0, using manual parse");
            dev->vendor_id = vid_le;
            dev->product_id = pid_le;
        } else {
            dev->vendor_id = desc->idVendor;
            dev->product_id = desc->idProduct;
        }
    } else {
        dev->vendor_id = desc->idVendor;
        dev->product_id = desc->idProduct;
    }
    
    if (!dev->has_hid) {
        dev->device_class = desc->bDeviceClass;
        dev->device_subclass = desc->bDeviceSubClass;
        dev->device_protocol = desc->bDeviceProtocol;
    }
    dev->num_configurations = desc->bNumConfigurations;

    klog_printf(KLOG_INFO, "usb_device: final VID:PID=%04x:%04x Class=%02x:%02x:%02x numConfigs=%d",
                dev->vendor_id, dev->product_id,
                dev->device_class, dev->device_subclass, dev->device_protocol,
                dev->num_configurations);

    return 0;
}

/**
 * Enumerate device (full enumeration process)
 */
int usb_device_enumerate(usb_device_t *dev) {
    if (!dev || !dev->controller) {
        klog_printf(KLOG_ERROR, "usb_device: invalid device for enumeration");
        return -1;
    }

    klog_printf(KLOG_INFO, "usb_device: enumerating device on controller %s",
                dev->controller->name);

    /* For XHCI: Enable Slot and Address Device */
    if (dev->controller->type == USB_CONTROLLER_XHCI) {
        if (dev->slot_id == 0) {
            xhci_controller_t *xhci = (xhci_controller_t *)dev->controller->private_data;
            if (!xhci) {
                klog_printf(KLOG_ERROR, "usb_device: XHCI controller private data is NULL");
                return -1;
            }
            
            /* Step 0: Enable Slot */
            extern uint32_t xhci_enable_slot(xhci_controller_t *xhci);
            uint32_t slot_id = xhci_enable_slot(xhci);
            if (slot_id == 0) {
                klog_printf(KLOG_ERROR, "usb_device: failed to enable slot");
                return -1;
            }
            dev->slot_id = slot_id;
            klog_printf(KLOG_INFO, "usb_device: slot %u enabled", slot_id);
        }
    }

    /* Step 1: Address Device (XHCI only - must be done before any transfers) */
    if (dev->controller->type == USB_CONTROLLER_XHCI && dev->slot_id > 0) {
        xhci_controller_t *xhci = (xhci_controller_t *)dev->controller->private_data;
        if (xhci) {
            /* Allocate Input Context */
            extern xhci_input_context_t *xhci_input_context_alloc(void);
            extern void xhci_input_context_set_slot(xhci_input_context_t *ctx, uint8_t slot_id, uint8_t root_port,
                                                    uint8_t speed, uint8_t address, bool hub, uint8_t parent_slot,
                                                    uint8_t parent_port);
            extern void xhci_input_context_set_ep0(xhci_input_context_t *ctx, xhci_transfer_ring_t *transfer_ring,
                                                    uint16_t max_packet_size);
            extern phys_addr_t xhci_input_context_get_phys(xhci_input_context_t *ctx);
            extern int xhci_address_device(xhci_controller_t *xhci, uint32_t slot_id, xhci_input_context_t *input_ctx,
                                            uint64_t input_ctx_phys);
            
            xhci_input_context_t *input_ctx = xhci_input_context_alloc();
            if (!input_ctx) {
                klog_printf(KLOG_ERROR, "usb_device: failed to allocate input context");
                return -1;
            }
            
            /* Determine speed (default to High Speed for now) */
            uint8_t speed = XHCI_SPEED_HIGH;
            if (dev->speed == USB_SPEED_FULL) speed = XHCI_SPEED_FULL;
            else if (dev->speed == USB_SPEED_LOW) speed = XHCI_SPEED_LOW;
            else if (dev->speed == USB_SPEED_SUPER) speed = XHCI_SPEED_SUPER;
            
            /* Set Slot Context */
            xhci_input_context_set_slot(input_ctx, dev->slot_id, dev->port, speed, 0, false, 0, 0);
            
            /* Initialize transfer ring for EP0 */
            extern int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
            if (xhci_transfer_ring_init(xhci, dev->slot_id, 0) < 0) {
                klog_printf(KLOG_ERROR, "usb_device: failed to init transfer ring for EP0");
                extern void xhci_input_context_free(xhci_input_context_t *ctx);
                xhci_input_context_free(input_ctx);
                return -1;
            }
            
            xhci_transfer_ring_t *transfer_ring = xhci->transfer_rings[dev->slot_id][0];
            if (!transfer_ring) {
                klog_printf(KLOG_ERROR, "usb_device: transfer ring not initialized");
                extern void xhci_input_context_free(xhci_input_context_t *ctx);
                xhci_input_context_free(input_ctx);
                return -1;
            }
            
            /* Set EP0 Context */
            xhci_input_context_set_ep0(input_ctx, transfer_ring, 64); /* Max packet size 64 for control */
            
            /* Get physical address */
            phys_addr_t input_ctx_phys = xhci_input_context_get_phys(input_ctx);
            if (input_ctx_phys == 0) {
                klog_printf(KLOG_ERROR, "usb_device: failed to get physical address for input context");
                extern void xhci_input_context_free(xhci_input_context_t *ctx);
                xhci_input_context_free(input_ctx);
                return -1;
            }
            
            /* Send Address Device command */
            if (xhci_address_device(xhci, dev->slot_id, input_ctx, input_ctx_phys) < 0) {
                klog_printf(KLOG_ERROR, "usb_device: Address Device command failed");
                extern void xhci_input_context_free(xhci_input_context_t *ctx);
                xhci_input_context_free(input_ctx);
                return -1;
            }
            
            /* Free input context (no longer needed after Address Device) */
            extern void xhci_input_context_free(xhci_input_context_t *ctx);
            xhci_input_context_free(input_ctx);
            
            klog_printf(KLOG_INFO, "usb_device: Address Device completed, EP0 is now active");
        }
    }
    
    /* Step 2: Get device descriptor at address 0 (default address) */
    /* Ensure device is at default address before enumeration */
    if (dev->address != 0) {
        klog_printf(KLOG_WARN, "usb_device: device already has address %d, resetting to 0", dev->address);
        dev->address = 0;
        dev->state = USB_DEVICE_STATE_DEFAULT;
    }
    
    int ret = usb_device_get_device_descriptor(dev);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get device descriptor at default address");
        return -1;
    }

    /* Step 2: Set address (allocate new address if address is 0) */
    ret = usb_device_set_address(dev, 0);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to set address");
        return -1;
    }
    
    /* Wait for device to accept new address (USB spec requires 2ms) */
    /* Small delay to ensure device has processed SET_ADDRESS */
    extern void pit_wait_ms(uint32_t ms);
    pit_wait_ms(2);

    /* Step 3: Get full device descriptor at new address (PO SET_ADDRESS) */
    /* This gives us correct num_configurations for subsequent operations */
    klog_printf(KLOG_INFO, "usb_device: getting device descriptor at new address %d",
                dev->address);
    ret = usb_device_get_device_descriptor(dev);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get device descriptor after address");
        return -1;
    }
    klog_printf(KLOG_INFO, "usb_device: device descriptor read - Class=%02x:%02x:%02x num_configs=%d",
                dev->device_class, dev->device_subclass, dev->device_protocol, dev->num_configurations);

    /* Step 4: Get configuration descriptors (PO SET_ADDRESS and PO device descriptor read) */
    /* Initialize HID fields before parsing */
    dev->has_hid = false;
    
    /* If device_class is already HID (0x03) from device descriptor, set has_hid */
    if (dev->device_class == 0x03) {
        dev->has_hid = true;
        klog_printf(KLOG_INFO, "usb_device: device-level HID class detected (0x03), setting has_hid=true");
    }
    k_memset(&dev->hid_interface, 0, sizeof(dev->hid_interface));
    k_memset(&dev->hid_desc, 0, sizeof(dev->hid_desc));
    k_memset(&dev->hid_intr_endpoint, 0, sizeof(dev->hid_intr_endpoint));
    
    klog_printf(KLOG_INFO, "usb_device: getting configuration descriptors (num_configs=%d)",
                dev->num_configurations);
    ret = usb_device_get_configurations(dev);
    if (ret < 0) {
        klog_printf(KLOG_ERROR, "usb_device: failed to get configurations");
        return -1;
    }

    /* Step 5: Parse interfaces and save HID interface class information */
    /* This must happen AFTER getting configurations and AFTER device descriptor read */
    klog_printf(KLOG_INFO, "usb_device: Step 5 - parsing interfaces (num_configs=%d address=%d state=%d)",
                dev->num_configurations, dev->address, dev->state);
    if (dev->num_configurations > 0) {
        klog_printf(KLOG_INFO, "usb_device: parsing interfaces for HID detection...");
        
        /* Get first configuration descriptor */
        uint8_t config_buffer[9];
        klog_printf(KLOG_INFO, "usb_device: getting configuration descriptor (index=0 address=%d)",
                    dev->address);
        ret = usb_get_descriptor(dev, USB_DT_CONFIGURATION, 0, 0, config_buffer, 9);
        klog_printf(KLOG_INFO, "usb_device: config descriptor read ret=%d", ret);
        if (ret >= 0) {
            usb_configuration_descriptor_t config;
            if (usb_parse_configuration_descriptor(config_buffer, 9, &config) == 0) {
                klog_printf(KLOG_INFO, "usb_device: config parsed - totalLength=%u numInterfaces=%u",
                            config.wTotalLength, config.bNumInterfaces);
                /* Get full configuration descriptor */
                uint8_t *full_config = (uint8_t *)kmalloc(config.wTotalLength);
                if (full_config) {
                    klog_printf(KLOG_INFO, "usb_device: reading full config descriptor (length=%u)",
                                config.wTotalLength);
                    ret = usb_get_descriptor(dev, USB_DT_CONFIGURATION, 0, 0, full_config, config.wTotalLength);
                    klog_printf(KLOG_INFO, "usb_device: full config descriptor read ret=%d", ret);
                    if (ret >= 0) {
                        size_t offset = config.bLength;
                        uint8_t interfaces_found = 0;
                        
                        klog_printf(KLOG_INFO, "usb_device: starting descriptor parsing (offset=%zu totalLength=%u)",
                                    offset, config.wTotalLength);
                        /* Parse all descriptors in configuration (INTERFACE, HID, ENDPOINT) */
                        while (offset + 2 < config.wTotalLength) {
                            uint8_t len = full_config[offset];
                            uint8_t type = full_config[offset + 1];
                            
                            klog_printf(KLOG_INFO, "usb_device: descriptor at offset=%zu len=%u type=0x%02x",
                                        offset, len, type);
                            
                            if (len == 0) {
                                klog_printf(KLOG_INFO, "usb_device: zero-length descriptor, stopping");
                                break;
                            }
                            
                            switch (type) {
                                case USB_DT_INTERFACE: {
                                    usb_interface_descriptor_t iface;
                                    if (usb_parse_interface_descriptor(full_config + offset, len, &iface) == 0) {
                                        interfaces_found++;
                                        klog_printf(KLOG_INFO,
                                            "usb_device: interface %d class=%02x subclass=%02x protocol=%02x",
                                            iface.bInterfaceNumber,
                                            iface.bInterfaceClass,
                                            iface.bInterfaceSubClass,
                                            iface.bInterfaceProtocol);
                                        
                                        /* Save HID interface class information */
                                        if (iface.bInterfaceClass == 0x03) { /* HID Class */
                                            klog_printf(KLOG_INFO, "usb_device: HID interface detected! class=0x%02x subclass=0x%02x protocol=0x%02x",
                                                        iface.bInterfaceClass, iface.bInterfaceSubClass, iface.bInterfaceProtocol);
                                            dev->device_class = 0x03;   /* MARK DEVICE AS HID */
                                            dev->device_protocol = iface.bInterfaceProtocol;
                                            dev->device_subclass = iface.bInterfaceSubClass;
                                            /* SAVE HID INTERFACE */
                                            memcpy(&dev->hid_interface, &iface, sizeof(iface));
                                            dev->has_hid = true;
                                            klog_printf(KLOG_INFO, "usb_device: device marked as HID (device_class=0x%02x has_hid=%d)",
                                                        dev->device_class, dev->has_hid ? 1 : 0);
                                        }
                                    }
                                } break;
                                
                                case USB_DT_HID: {
                                    usb_hid_descriptor_t hid;
                                    if (usb_parse_hid_descriptor(full_config + offset, len, &hid) == 0) {
                                        /* SAVE HID DESCRIPTOR */
                                        memcpy(&dev->hid_desc, &hid, sizeof(hid));
                                        klog_printf(KLOG_INFO,
                                            "usb_device: HID descriptor reportLen=%u country=%u descriptors=%u",
                                            hid.wDescriptorLength,
                                            hid.bCountryCode,
                                            hid.bNumDescriptors);
                                    }
                                } break;
                                
                                case USB_DT_ENDPOINT: {
                                    usb_endpoint_descriptor_t ep;
                                    if (usb_parse_endpoint_descriptor(full_config + offset, len, &ep) == 0) {
                                        /* Check if this is an interrupt IN endpoint (for HID) */
                                        if ((ep.bEndpointAddress & 0x80) && (ep.bmAttributes & 0x03) == 0x03) {
                                            /* SAVE HID INTERRUPT IN ENDPOINT */
                                            memcpy(&dev->hid_intr_endpoint, &ep, sizeof(ep));
                                            klog_printf(KLOG_INFO,
                                                "usb_device: HID interrupt endpoint found: addr=%02x size=%u interval=%u",
                                                ep.bEndpointAddress, ep.wMaxPacketSize, ep.bInterval);
                                        }
                                    }
                                } break;
                            }
                            
                            offset += len;
                        }
                        
                        klog_printf(KLOG_INFO, "usb_device: parsed %d interfaces, has_hid=%d device_class=0x%02x",
                                    interfaces_found, dev->has_hid ? 1 : 0, dev->device_class);
                    }
                    kfree(full_config);
                }
            }
        }
    } else {
        klog_printf(KLOG_INFO, "usb_device: no configurations available, skipping interface parsing");
    }

    /* Step 6: Set configuration (usually configuration 1) */
    if (dev->num_configurations > 0) {
        klog_printf(KLOG_INFO, "usb_device: setting configuration 1 (device_class=0x%02x has_hid=%d)",
                    dev->device_class, dev->has_hid ? 1 : 0);
        ret = usb_device_set_configuration(dev, 1);
        if (ret < 0) {
            klog_printf(KLOG_WARN, "usb_device: failed to set configuration 1");
            /* Continue anyway */
        } else {
            klog_printf(KLOG_INFO, "usb_device: configuration set successfully, device state=%d",
                        dev->state);
        }
    }

    /* Add to device list */
    usb_device_list_add(dev);

    klog_printf(KLOG_INFO, "usb_device: enumeration complete (address=%d device_class=0x%02x has_hid=%d)",
                dev->address, dev->device_class, dev->has_hid ? 1 : 0);
    
    /* Bind driver after enumeration */
    extern int usb_bind_driver(usb_device_t *dev);
    usb_bind_driver(dev);
    
    return 0;
}

/**
 * Bind driver to device after enumeration
 * 
 * This function tries to find and initialize appropriate driver for the device
 * by matching against registered USB drivers.
 */
int usb_bind_driver(usb_device_t *dev) {
    if (!dev) {
        klog_printf(KLOG_ERROR, "usb_bind: invalid device");
        return -1;
    }

    klog_printf(KLOG_INFO, "usb_bind: binding driver for device VID:PID=%04x:%04x Class=%02x:%02x:%02x",
                dev->vendor_id, dev->product_id, 
                dev->device_class, dev->device_subclass, dev->device_protocol);

    /* Try all registered drivers */
    extern int usb_get_driver_count(void);
    extern usb_driver_t *usb_get_driver(int index);
    
    int driver_count = usb_get_driver_count();
    for (int i = 0; i < driver_count; i++) {
        usb_driver_t *drv = usb_get_driver(i);
        if (!drv || !drv->probe) continue;
        
        /* Try to probe device */
        if (drv->probe(dev) == 0) {
            klog_printf(KLOG_INFO, "usb_bind: driver %s matches device %04x:%04x",
                        drv->name, dev->vendor_id, dev->product_id);
            
            /* Initialize driver */
            if (drv->init && drv->init(dev) == 0) {
                klog_printf(KLOG_INFO, "usb_bind: driver %s initialized successfully", drv->name);
                return 0;
            } else {
                klog_printf(KLOG_WARN, "usb_bind: driver %s probe succeeded but init failed", drv->name);
            }
        }
    }

    klog_printf(KLOG_WARN, "usb_bind: no driver found for device Class=%02x:%02x:%02x",
                dev->device_class, dev->device_subclass, dev->device_protocol);
    return -1;
}

