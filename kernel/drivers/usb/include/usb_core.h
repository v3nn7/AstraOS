#ifndef USB_CORE_H
#define USB_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_controller.h"
#include "usb_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* High-level USB stack bookkeeping for detected controllers and devices. */
typedef struct usb_stack_state {
    usb_controller_t *controllers[8];
    uint32_t controller_count;
    usb_device_t *devices[32];
    uint32_t device_count;
} usb_stack_state_t;

/* Initialize the global USB stack state with basic controller stubs. */
void usb_stack_init(void);

/* Poll controllers and devices; stubbed for now for future expansion. */
void usb_stack_poll(void);

/* Accessors for the current global USB stack state. */
const usb_stack_state_t *usb_stack_state(void);
uint32_t usb_stack_controller_count(void);
uint32_t usb_stack_device_count(void);
const usb_controller_t *usb_stack_controller_at(uint32_t index);
bool usb_stack_add_device(usb_controller_t* ctrl, usb_speed_t speed);
const usb_device_t* usb_stack_device_at(uint32_t index);
bool usb_stack_enumerate_basic();
void usb_stack_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_CORE_H */
