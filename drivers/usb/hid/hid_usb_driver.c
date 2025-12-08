/**
 * USB HID Driver Registration
 * 
 * Registers HID keyboard and mouse drivers with USB driver system.
 * 
 * FLEXIBLE MATCHING STRATEGY:
 * Real-world HID devices often have non-standard descriptors.
 * This implementation uses multiple fallback strategies to ensure compatibility.
 */

 #include "usb/usb_core.h"
 #include "usb/usb_hid.h"
 #include "kmalloc.h"
 #include "klog.h"
 
 /* Forward declarations */
 extern int usb_hid_keyboard_init(usb_device_t *dev);
 extern int usb_hid_mouse_init(usb_device_t *dev);
 extern void usb_hid_register_keyboard(usb_device_t *dev);
 extern void usb_hid_register_mouse(usb_device_t *dev);
 
 /* Global flags for USB HID device detection */
 extern bool usb_hid_keyboard_found;
 extern bool usb_hid_mouse_found;
 
 /**
  * Probe for HID keyboard device
  * 
  * MATCHING STRATEGY (in order of priority):
  * 1. Device class=0x03 + has_hid + protocol=0x01 (standard keyboard)
  * 2. Interface class=0x03 + protocol=0x01 (interface-level keyboard)
  * 3. Device class=0x03 + flexible subclass/protocol
  * 4. Interface class=0x03 + boot interface
  * 5. FALLBACK: has_hid + class=0x03 (any HID device, will determine type from reports)
  */
 static int hid_keyboard_probe(usb_device_t *dev) {
     if (!dev) return -1;
     
     klog_printf(KLOG_INFO, "hid_driver: keyboard probe - class=0x%02x subclass=0x%02x protocol=0x%02x has_hid=%d state=%d",
                 dev->device_class, dev->device_subclass, dev->device_protocol, dev->has_hid ? 1 : 0, dev->state);
     klog_printf(KLOG_INFO, "hid_driver: keyboard probe - iface_class=0x%02x iface_subclass=0x%02x iface_protocol=0x%02x",
                 dev->hid_interface.bInterfaceClass, dev->hid_interface.bInterfaceSubClass, dev->hid_interface.bInterfaceProtocol);
     
     /* Device must be in CONFIGURED state */
     if (dev->state != USB_DEVICE_STATE_CONFIGURED) {
         klog_printf(KLOG_INFO, "hid_driver: keyboard probe failed - device not configured (state=%d)", dev->state);
         return -1;
     }
     
     /* STRATEGY 1: Standard HID keyboard (device level) */
     if (dev->has_hid && dev->device_class == 0x03) {
         if (dev->device_protocol == 0x01) {
             klog_printf(KLOG_INFO, "hid_driver: keyboard MATCH [Strategy 1] (device: HID + has_hid + protocol=0x01)");
             return 0;
         }
     }
     
     /* STRATEGY 2: Interface-level HID keyboard */
     if (dev->hid_interface.bInterfaceClass == 0x03) {
         /* Standard keyboard protocol */
         if (dev->hid_interface.bInterfaceProtocol == 0x01) {
             if (dev->hid_interface.bInterfaceSubClass == 0x00 || dev->hid_interface.bInterfaceSubClass == 0x01) {
                 klog_printf(KLOG_INFO, "hid_driver: keyboard MATCH [Strategy 2] (interface: class=0x03 + protocol=0x01)");
                 return 0;
             }
         }
         /* Boot interface with generic protocol */
         if (dev->hid_interface.bInterfaceSubClass == 0x01 && dev->hid_interface.bInterfaceProtocol == 0x00) {
             klog_printf(KLOG_INFO, "hid_driver: keyboard MATCH [Strategy 2b] (interface: boot subclass + protocol=0x00)");
             return 0;
         }
     }
     
     /* STRATEGY 3: Device-level HID with flexible subclass/protocol */
     if (dev->device_class == 0x03) {
         /* Accept subclass 0x00 (No Subclass) or 0x01 (Boot Interface) */
         if (dev->device_subclass == 0x00 || dev->device_subclass == 0x01) {
             /* Accept protocol 0x00 (None) or 0x01 (Keyboard) */
             if (dev->device_protocol == 0x00 || dev->device_protocol == 0x01) {
                 klog_printf(KLOG_INFO, "hid_driver: keyboard MATCH [Strategy 3] (device: HID + flexible subclass/protocol)");
                 return 0;
             }
         }
     }
     
     /* STRATEGY 4: has_hid flag with interface-level check */
     if (dev->has_hid) {
         if (dev->hid_interface.bInterfaceClass == 0x03) {
             if (dev->hid_interface.bInterfaceSubClass == 0x01) {
                 if (dev->hid_interface.bInterfaceProtocol == 0x01 || dev->hid_interface.bInterfaceProtocol == 0x00) {
                     klog_printf(KLOG_INFO, "hid_driver: keyboard MATCH [Strategy 4] (has_hid + interface boot)");
                     return 0;
                 }
             }
         }
     }
     
     /* STRATEGY 5: AGGRESSIVE FALLBACK - Accept any HID device and determine type from reports */
     /* This handles devices with corrupted/non-standard descriptors */
     if (dev->has_hid && dev->device_class == 0x03) {
         /* Only accept if mouse hasn't been found yet to avoid double-matching */
         if (!usb_hid_mouse_found) {
             klog_printf(KLOG_WARN, "hid_driver: keyboard MATCH [Strategy 5 - FALLBACK] (HID device with non-standard descriptor, will analyze reports)");
             klog_printf(KLOG_WARN, "hid_driver: Non-standard descriptor: class=0x%02x subclass=0x%02x protocol=0x%02x",
                         dev->device_class, dev->device_subclass, dev->device_protocol);
             return 0;
         }
     }
     
     klog_printf(KLOG_INFO, "hid_driver: keyboard probe NO MATCH");
     return -1;
 }
 
 /**
  * Initialize HID keyboard driver
  */
 static int hid_keyboard_init(usb_device_t *dev) {
     if (!dev) return -1;
     
     klog_printf(KLOG_INFO, "hid_driver: initializing keyboard driver");
     
     if (usb_hid_keyboard_init(dev) == 0) {
         usb_hid_register_keyboard(dev);
         usb_hid_keyboard_found = true; /* Mark keyboard as found */
         klog_printf(KLOG_INFO, "hid_driver: keyboard driver initialized successfully");
         return 0;
     }
     
     klog_printf(KLOG_ERROR, "hid_driver: keyboard driver initialization failed");
     return -1;
 }
 
 /**
  * Probe for HID mouse device
  * 
  * MATCHING STRATEGY (in order of priority):
  * 1. Device class=0x03 + has_hid + protocol=0x02 (standard mouse)
  * 2. Interface class=0x03 + protocol=0x02 (interface-level mouse)
  * 3. Device class=0x03 + flexible subclass/protocol
  * 4. Interface class=0x03 + boot interface (if keyboard already found)
  * 5. FALLBACK: has_hid + class=0x03 (any HID device not claimed by keyboard)
  */
 static int hid_mouse_probe(usb_device_t *dev) {
     if (!dev) return -1;
     
     klog_printf(KLOG_INFO, "hid_driver: mouse probe - class=0x%02x subclass=0x%02x protocol=0x%02x has_hid=%d state=%d",
                 dev->device_class, dev->device_subclass, dev->device_protocol, dev->has_hid ? 1 : 0, dev->state);
     klog_printf(KLOG_INFO, "hid_driver: mouse probe - iface_class=0x%02x iface_subclass=0x%02x iface_protocol=0x%02x",
                 dev->hid_interface.bInterfaceClass, dev->hid_interface.bInterfaceSubClass, dev->hid_interface.bInterfaceProtocol);
     
     /* Device must be in CONFIGURED state */
     if (dev->state != USB_DEVICE_STATE_CONFIGURED) {
         klog_printf(KLOG_INFO, "hid_driver: mouse probe failed - device not configured (state=%d)", dev->state);
         return -1;
     }
     
     /* STRATEGY 1: Standard HID mouse (device level) */
     if (dev->has_hid && dev->device_class == 0x03) {
         if (dev->device_protocol == 0x02) {
             klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 1] (device: HID + has_hid + protocol=0x02)");
             return 0;
         }
     }
     
     /* STRATEGY 2: Interface-level HID mouse */
     if (dev->hid_interface.bInterfaceClass == 0x03) {
         /* Standard mouse protocol */
         if (dev->hid_interface.bInterfaceProtocol == 0x02) {
             if (dev->hid_interface.bInterfaceSubClass == 0x00 || dev->hid_interface.bInterfaceSubClass == 0x01) {
                 klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 2] (interface: class=0x03 + protocol=0x02)");
                 return 0;
             }
         }
         /* Boot interface with generic protocol (only if keyboard already claimed) */
         if (dev->hid_interface.bInterfaceSubClass == 0x01 && dev->hid_interface.bInterfaceProtocol == 0x00) {
             if (usb_hid_keyboard_found) {
                 klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 2b] (interface: boot + protocol=0x00, keyboard exists)");
                 return 0;
             }
         }
     }
     
     /* STRATEGY 3: Device-level HID with flexible subclass/protocol */
     if (dev->device_class == 0x03) {
         /* Accept subclass 0x00 (No Subclass) or 0x01 (Boot Interface) */
         if (dev->device_subclass == 0x00 || dev->device_subclass == 0x01) {
             /* Prefer protocol 0x02 (Mouse) */
             if (dev->device_protocol == 0x02) {
                 klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 3] (device: HID + protocol=0x02)");
                 return 0;
             }
             /* Accept protocol 0x00 only if keyboard already found */
             if (dev->device_protocol == 0x00 && usb_hid_keyboard_found) {
                 klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 3b] (device: HID + protocol=0x00, keyboard exists)");
                 return 0;
             }
         }
     }
     
     /* STRATEGY 4: has_hid flag with interface-level check */
     if (dev->has_hid) {
         if (dev->hid_interface.bInterfaceClass == 0x03) {
             if (dev->hid_interface.bInterfaceSubClass == 0x01) {
                 if (dev->hid_interface.bInterfaceProtocol == 0x02) {
                     klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 4] (has_hid + interface boot + protocol=0x02)");
                     return 0;
                 }
                 /* Generic protocol only if keyboard found */
                 if (dev->hid_interface.bInterfaceProtocol == 0x00 && usb_hid_keyboard_found) {
                     klog_printf(KLOG_INFO, "hid_driver: mouse MATCH [Strategy 4b] (has_hid + interface boot + protocol=0x00, keyboard exists)");
                     return 0;
                 }
             }
         }
     }
     
     /* STRATEGY 5: AGGRESSIVE FALLBACK - Accept any remaining HID device */
     /* This handles devices with corrupted/non-standard descriptors */
     /* Only match if keyboard already found to avoid stealing its device */
     if (dev->has_hid && dev->device_class == 0x03 && usb_hid_keyboard_found) {
         klog_printf(KLOG_WARN, "hid_driver: mouse MATCH [Strategy 5 - FALLBACK] (HID device with non-standard descriptor, keyboard exists)");
         klog_printf(KLOG_WARN, "hid_driver: Non-standard descriptor: class=0x%02x subclass=0x%02x protocol=0x%02x",
                     dev->device_class, dev->device_subclass, dev->device_protocol);
         return 0;
     }
     
     klog_printf(KLOG_INFO, "hid_driver: mouse probe NO MATCH");
     return -1;
 }
 
 /**
  * Initialize HID mouse driver
  */
 static int hid_mouse_init(usb_device_t *dev) {
     if (!dev) return -1;
     
     klog_printf(KLOG_INFO, "hid_driver: initializing mouse driver");
     
     if (usb_hid_mouse_init(dev) == 0) {
         usb_hid_register_mouse(dev);
         usb_hid_mouse_found = true; /* Mark mouse as found */
         klog_printf(KLOG_INFO, "hid_driver: mouse driver initialized successfully");
         return 0;
     }
     
     klog_printf(KLOG_ERROR, "hid_driver: mouse driver initialization failed");
     return -1;
 }
 
 /* USB HID Keyboard Driver */
 static usb_driver_t usb_hid_keyboard_driver = {
     .name = "usb-hid-keyboard",
     .probe = hid_keyboard_probe,
     .init = hid_keyboard_init,
     .remove = NULL
 };
 
 /* USB HID Mouse Driver */
 static usb_driver_t usb_hid_mouse_driver = {
     .name = "usb-hid-mouse",
     .probe = hid_mouse_probe,
     .init = hid_mouse_init,
     .remove = NULL
 };
 
 /**
  * Register HID drivers
  */
 void usb_hid_register_drivers(void) {
     extern int usb_register_driver(usb_driver_t *drv);
     
     /* Register keyboard driver first to give it priority */
     usb_register_driver(&usb_hid_keyboard_driver);
     usb_register_driver(&usb_hid_mouse_driver);
     
     klog_printf(KLOG_INFO, "hid_driver: registered HID keyboard and mouse drivers");
 }