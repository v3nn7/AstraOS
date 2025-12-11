#include "event.h"
#include "klog.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include <drivers/usb/usb_hub.h>

extern "C" {

#include "interrupts.h"

volatile uint32_t *apic_lapic_base = nullptr;

void *k_memset(void *dest, int value, unsigned long n) {
    unsigned char *d = static_cast<unsigned char *>(dest);
    for (unsigned long i = 0; i < n; ++i) d[i] = static_cast<unsigned char>(value);
    return dest;
}

void *memcpy(void *dst, const void *src, unsigned long n) {
    auto *d = static_cast<unsigned char *>(dst);
    const auto *s = static_cast<const unsigned char *>(src);
    for (unsigned long i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

int usb_hub_register_driver(usb_driver_t *) { return 0; }

void irq_register(uint8_t, irq_handler_t) {}

bool gui_event_poll(gui_event_t *) { return false; }
void gui_event_push_keychar(char) {}
void gui_event_push_mouse_move(int32_t, int32_t, int32_t, int32_t, uint8_t) {}
void gui_event_push_mouse_scroll(int32_t, int32_t, int32_t) {}

uint32_t fb_width(void) { return 800; }
uint32_t fb_height(void) { return 600; }
void mouse_cursor_draw(int32_t, int32_t) {}

void irq_register_handler(uint8_t, irq_handler_t) {}

} // extern "C"

void *operator new(unsigned long long sz) { return kmalloc(static_cast<size_t>(sz)); }
void *operator new[](unsigned long long sz) { return kmalloc(static_cast<size_t>(sz)); }
void operator delete(void *p, unsigned long long) noexcept { kfree(p); }
void operator delete(void *p) noexcept { kfree(p); }
void operator delete[](void *p, unsigned long long) noexcept { kfree(p); }
void operator delete[](void *p) noexcept { kfree(p); }
