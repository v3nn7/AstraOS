#include "hid.h"
#include "kernel.h"
#include "usb.h"
#include "interrupts.h"

void hid_init(void) {
    int ctrls = usb_controller_count();
    if (ctrls == 0) {
        printf("HID: no USB controllers, skipping\n");
        return;
    }

    /* Wait up to ~5s for HID presence (rough busy-wait), then skip */
    const uint64_t limit = 5ULL * 1000000ULL; // crude loop count
    uint64_t i = 0;
    for (; i < limit; ++i) {
        __asm__ volatile("pause");
    }

    printf("HID: no devices detected within 5s, skipping\n");
}

