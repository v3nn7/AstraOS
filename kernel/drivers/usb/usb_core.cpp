#include "usb_core.hpp"

#include <stddef.h>

#include "util/logger.hpp"
#include "drivers/ps2/ps2.hpp"
#include "drivers/usb/include/xhci.h"
#include "drivers/usb/include/usb_defs.h"
#include "drivers/usb/include/usb_controller.h"
#include "drivers/usb/include/xhci_regs.h"
#include "drivers/usb/include/msi.h"

extern "C" bool pci_find_xhci(uintptr_t* base, uint8_t* bus, uint8_t* slot, uint8_t* func);

namespace {

void append_str(char* dst, size_t& len, size_t cap, const char* src) {
    while (len + 1 < cap && *src) {
        dst[len++] = *src++;
    }
    dst[len] = 0;
}

void append_uint(char* dst, size_t& len, size_t cap, uint32_t value) {
    char buf[11];
    size_t digits = 0;
    do {
        buf[digits++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value && digits < sizeof(buf));

    while (digits > 0 && len + 1 < cap) {
        dst[len++] = buf[--digits];
    }
    dst[len] = 0;
}

}  // namespace

namespace usb {

void usb_init() {
    // BIOS handoff prerequisites: disable legacy PS/2 and claim USB ownership.
    ps2::disable_legacy();

    usb_stack_init();

    uintptr_t xhci_mmio = 0;
    uint8_t bus = 0, slot = 0, func = 0;
    bool found = pci_find_xhci(&xhci_mmio, &bus, &slot, &func);
    if (found && xhci_mmio != 0) {
        usb_controller_t* ctrl = (usb_controller_t*)usb_stack_controller_at(0);
        xhci_controller_t* xhci = ctrl ? (xhci_controller_t*)ctrl->driver_data : nullptr;
        if (xhci && xhci_controller_init(xhci, xhci_mmio)) {
            uint8_t slot = 0;
            xhci_cmd_enable_slot(xhci, &slot);
            xhci_cmd_address_device(xhci, slot, 0);
            xhci_cmd_configure_endpoint(xhci, slot, 0);
            usb_stack_enumerate_basic();
            if (!xhci_check_lowmem(xhci_mmio)) {
                klog("xhci: warning BAR above 4GB; ensure mapping before DMA");
            }
        } else {
            klog("xhci: init failed");
        }
    } else {
        klog("xhci: controller not found");
    }

    const usb_stack_state_t* state = usb_stack_state();
    char msg[64];
    size_t len = 0;
    msg[0] = 0;
    append_str(msg, len, sizeof(msg), "USB init: controllers=");
    append_uint(msg, len, sizeof(msg), state ? state->controller_count : 0);
    append_str(msg, len, sizeof(msg), " devices=");
    append_uint(msg, len, sizeof(msg), state ? state->device_count : 0);
    klog(msg);
}

void usb_poll() {
    usb_stack_poll();
    /* For now, also keep shell-visible counts in sync via poll path. */
}

uint32_t controller_count() {
    return usb_stack_controller_count();
}

uint32_t device_count() {
    return usb_stack_device_count();
}

const usb_controller_t* controller_at(uint32_t index) {
    return usb_stack_controller_at(index);
}

}  // namespace usb
