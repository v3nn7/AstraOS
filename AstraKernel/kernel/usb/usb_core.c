#include "usb_core.h"
#include "kernel.h"
#include "string.h"

static usb_controller_t controllers[8];
static int ctrl_count = 0;

static usb_device_t devices[16];
static int dev_count = 0;

void usb_register_controller(usb_controller_t c) {
    if (ctrl_count < 8)
        controllers[ctrl_count++] = c;
}

void usb_core_init(void) {
    printf("USB: core init, %d controllers\n", ctrl_count);
}
