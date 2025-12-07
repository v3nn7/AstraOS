#pragma once

#include "types.h"
#include "interrupts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB Core Layer
 * 
 * Provides unified USB device management, enumeration, and transfer scheduling
 * across all controller types (UHCI, OHCI, EHCI, XHCI).
 */

/* USB Specification Constants */
#define USB_VERSION_1_0    0x0100
#define USB_VERSION_1_1    0x0110
#define USB_VERSION_2_0    0x0200
#define USB_VERSION_3_0    0x0300
#define USB_VERSION_3_1    0x0310
#define USB_VERSION_3_2    0x0320

/* USB Request Types */
#define USB_REQ_TYPE_STANDARD    (0 << 5)
#define USB_REQ_TYPE_CLASS       (1 << 5)
#define USB_REQ_TYPE_VENDOR      (2 << 5)
#define USB_REQ_TYPE_RESERVED    (3 << 5)

#define USB_REQ_TYPE_DEVICE      0
#define USB_REQ_TYPE_INTERFACE   1
#define USB_REQ_TYPE_ENDPOINT    2
#define USB_REQ_TYPE_OTHER       3

/* USB Request Codes */
#define USB_REQ_GET_STATUS           0x00
#define USB_REQ_CLEAR_FEATURE        0x01
#define USB_REQ_SET_FEATURE          0x03
#define USB_REQ_SET_ADDRESS          0x05
#define USB_REQ_GET_DESCRIPTOR       0x06
#define USB_REQ_SET_DESCRIPTOR       0x07
#define USB_REQ_GET_CONFIGURATION    0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE        0x0A
#define USB_REQ_SET_INTERFACE        0x0B
#define USB_REQ_SYNCH_FRAME          0x0C

/* USB Descriptor Types */
#define USB_DT_DEVICE                0x01
#define USB_DT_CONFIGURATION         0x02
#define USB_DT_STRING                0x03
#define USB_DT_INTERFACE             0x04
#define USB_DT_ENDPOINT              0x05
#define USB_DT_DEVICE_QUALIFIER      0x06
#define USB_DT_OTHER_SPEED_CONFIG   0x07
#define USB_DT_INTERFACE_POWER       0x08
#define USB_DT_OTG                   0x09
#define USB_DT_DEBUG                 0x0A
#define USB_DT_INTERFACE_ASSOC       0x0B
#define USB_DT_BOS                   0x0F
#define USB_DT_DEVICE_CAPABILITY     0x10
#define USB_DT_HID                   0x21
#define USB_DT_HID_REPORT            0x22
#define USB_DT_HID_PHYSICAL          0x23
#define USB_DT_CS_INTERFACE          0x24
#define USB_DT_CS_ENDPOINT           0x25
#define USB_DT_HUB                   0x29

/* USB Endpoint Types */
#define USB_ENDPOINT_XFER_CONTROL    0
#define USB_ENDPOINT_XFER_ISOC       1
#define USB_ENDPOINT_XFER_BULK       2
#define USB_ENDPOINT_XFER_INT         3

/* USB Endpoint Directions */
#define USB_ENDPOINT_DIR_OUT          0
#define USB_ENDPOINT_DIR_IN           0x80

/* USB Transfer Status */
typedef enum {
    USB_TRANSFER_SUCCESS = 0,
    USB_TRANSFER_ERROR,
    USB_TRANSFER_STALL,
    USB_TRANSFER_TIMEOUT,
    USB_TRANSFER_NO_DEVICE,
    USB_TRANSFER_BABBLE,
    USB_TRANSFER_CRC_ERROR,
    USB_TRANSFER_SHORT_PACKET
} usb_transfer_status_t;

/* USB Device Speed */
typedef enum {
    USB_SPEED_UNKNOWN = 0,
    USB_SPEED_LOW,      /* 1.5 Mbps */
    USB_SPEED_FULL,     /* 12 Mbps */
    USB_SPEED_HIGH,     /* 480 Mbps */
    USB_SPEED_SUPER,    /* 5 Gbps */
    USB_SPEED_SUPER_PLUS /* 10 Gbps */
} usb_speed_t;

/* USB Controller Type */
typedef enum {
    USB_CONTROLLER_UHCI = 0,
    USB_CONTROLLER_OHCI,
    USB_CONTROLLER_EHCI,
    USB_CONTROLLER_XHCI
} usb_controller_type_t;

/* Forward declarations */
typedef struct usb_device usb_device_t;
typedef struct usb_host_controller usb_host_controller_t;
typedef struct usb_transfer usb_transfer_t;
typedef struct usb_endpoint usb_endpoint_t;

/* USB Transfer Callback */
typedef void (*usb_transfer_callback_t)(usb_transfer_t *transfer);

/* USB Transfer Structure */
struct usb_transfer {
    usb_device_t *device;
    usb_endpoint_t *endpoint;
    uint8_t *buffer;
    size_t length;
    size_t actual_length;
    usb_transfer_status_t status;
    usb_transfer_callback_t callback;
    void *user_data;
    uint32_t timeout_ms;
    bool is_control;
    uint8_t setup[8]; /* Control transfer setup packet */
    void *controller_private; /* Controller-specific data */
};

/* USB Endpoint */
struct usb_endpoint {
    usb_device_t *device;
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
    uint8_t type;
    bool toggle;
    void *controller_private;
};

/* USB Host Controller Operations */
typedef struct {
    int (*init)(usb_host_controller_t *hc);
    int (*reset)(usb_host_controller_t *hc);
    int (*reset_port)(usb_host_controller_t *hc, uint8_t port);
    int (*transfer_control)(usb_host_controller_t *hc, usb_transfer_t *transfer);
    int (*transfer_interrupt)(usb_host_controller_t *hc, usb_transfer_t *transfer);
    int (*transfer_bulk)(usb_host_controller_t *hc, usb_transfer_t *transfer);
    int (*transfer_isoc)(usb_host_controller_t *hc, usb_transfer_t *transfer);
    int (*poll)(usb_host_controller_t *hc); /* For polling-based controllers */
    void (*cleanup)(usb_host_controller_t *hc);
} usb_host_ops_t;

/* USB Host Controller */
struct usb_host_controller {
    usb_controller_type_t type;
    const char *name;
    void *regs_base; /* MMIO base address */
    uint32_t irq;
    uint8_t num_ports;
    bool enabled;
    usb_host_ops_t *ops;
    void *private_data; /* Controller-specific data */
    usb_device_t *root_hub;
    struct usb_host_controller *next;
};

/* USB Device States */
typedef enum {
    USB_DEVICE_STATE_DEFAULT = 0,
    USB_DEVICE_STATE_ADDRESS,
    USB_DEVICE_STATE_CONFIGURED,
    USB_DEVICE_STATE_SUSPENDED,
    USB_DEVICE_STATE_DISCONNECTED
} usb_device_state_t;

/* USB Device Structure */
struct usb_device {
    uint8_t address;
    usb_speed_t speed;
    usb_device_state_t state;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t num_configurations;
    uint8_t active_configuration;
    usb_endpoint_t endpoints[32]; /* Max 32 endpoints per device */
    uint8_t num_endpoints;
    usb_host_controller_t *controller;
    usb_device_t *parent; /* For hubs */
    usb_device_t *children[32]; /* Max 32 child devices */
    uint8_t num_children;
    uint8_t port; /* Port number on parent hub */
    uint8_t slot_id; /* XHCI slot ID (0 = not allocated) */
    void *descriptors; /* Parsed descriptors */
    size_t descriptors_size;
    void *driver_data; /* Driver-specific data */
    struct usb_device *next;
};

/* USB Core Functions */
int usb_core_init(void);
void usb_core_cleanup(void);

/* Host Controller Management */
int usb_host_register(usb_host_controller_t *hc);
int usb_host_unregister(usb_host_controller_t *hc);
usb_host_controller_t *usb_host_find_by_type(usb_controller_type_t type);

/* Device Management */
usb_device_t *usb_device_alloc(void);
void usb_device_free(usb_device_t *dev);
int usb_device_enumerate(usb_device_t *dev);
int usb_device_set_address(usb_device_t *dev, uint8_t address);
int usb_device_set_configuration(usb_device_t *dev, uint8_t config);
usb_device_t *usb_device_find_by_address(uint8_t address);
usb_device_t *usb_device_find_by_vid_pid(uint16_t vid, uint16_t pid);

/* Transfer Functions */
usb_transfer_t *usb_transfer_alloc(usb_device_t *dev, usb_endpoint_t *ep, size_t length);
void usb_transfer_free(usb_transfer_t *transfer);
int usb_transfer_submit(usb_transfer_t *transfer);
int usb_transfer_cancel(usb_transfer_t *transfer);

/* Control Transfer Helpers */
int usb_control_transfer(usb_device_t *dev, uint8_t bmRequestType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex, void *data, uint16_t wLength,
                         uint32_t timeout_ms);

/* Descriptor Helpers */
int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       uint16_t lang_id, void *buffer, size_t length);
int usb_get_string_descriptor(usb_device_t *dev, uint8_t index, uint16_t lang_id,
                              char *buffer, size_t length);

/* Endpoint Management */
usb_endpoint_t *usb_endpoint_alloc(usb_device_t *dev, uint8_t address,
                                    uint8_t attributes, uint16_t max_packet_size,
                                    uint8_t interval);
void usb_endpoint_free(usb_endpoint_t *ep);

/* USB Driver System */
typedef struct usb_driver {
    const char *name;
    int (*probe)(usb_device_t *dev);  /* Check if driver matches device */
    int (*init)(usb_device_t *dev);   /* Initialize driver for device */
    void (*remove)(usb_device_t *dev); /* Cleanup driver */
} usb_driver_t;

/* Driver registration */
int usb_register_driver(usb_driver_t *drv);
int usb_bind_driver(usb_device_t *dev);
int usb_get_driver_count(void);
usb_driver_t *usb_get_driver(int index);

/* Debug/Logging */
void usb_debug_print_device(usb_device_t *dev);
void usb_debug_print_transfer(usb_transfer_t *transfer);

#ifdef __cplusplus
}
#endif

