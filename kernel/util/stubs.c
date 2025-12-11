#include "event.h"
#include "klog.h"
#include "memory.h"
#include "string.h"
#include "stdio.h"
#include "../drivers/serial.hpp"
#include "../util/logger.hpp"
#include <drivers/usb/usb_hub.h>

volatile uint32_t *apic_lapic_base = 0;

void *k_memset(void *dest, int value, unsigned long n) {
    unsigned char *d = dest;
    for (unsigned long i = 0; i < n; ++i) d[i] = (unsigned char)value;
    return dest;
}

void *memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (unsigned long i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

void klog_printf(klog_level_t level, const char *fmt, ...) {
    (void)level;
    serial_write("[LOG] ");
    serial_write(fmt);
    serial_write("\r\n");
    klog(fmt);
}

void klog_init(void) {}
void klog_set_level(klog_level_t level) { (void)level; }
klog_level_t klog_get_level(void) { return KLOG_INFO; }
const char *klog_level_name(klog_level_t level) { (void)level; return "info"; }
size_t klog_copy_recent(char *out, size_t max_len) { (void)out; return max_len; }

int usb_hub_register_driver(usb_driver_t *drv) { (void)drv; return 0; }

void irq_register(uint8_t vec, irq_handler_t handler) { (void)vec; (void)handler; }

bool gui_event_poll(gui_event_t *out) { (void)out; return false; }
void gui_event_push_keychar(char c) { (void)c; }
void gui_event_push_mouse_move(int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons) {
    (void)x; (void)y; (void)dx; (void)dy; (void)buttons;
}
void gui_event_push_mouse_scroll(int32_t x, int32_t y, int32_t delta) {
    (void)x; (void)y; (void)delta;
}

uint32_t fb_width(void) { return 800; }
uint32_t fb_height(void) { return 600; }
void mouse_cursor_draw(int32_t x, int32_t y) { (void)x; (void)y; }

/* simple C operators replacements for C++ new/delete expected by clang++ */
void *operator_new(unsigned long long sz) { return kmalloc((size_t)sz); }
void *operator_new_array(unsigned long long sz) { return kmalloc((size_t)sz); }
void operator_delete(void *p, unsigned long long) { kfree(p); }
void operator_delete2(void *p) { kfree(p); }
void operator_delete_array(void *p, unsigned long long) { kfree(p); }
void operator_delete_array2(void *p) { kfree(p); }

/* Provide the expected mangled names for MSVC-style COFF (clang++) */
void *operator new(unsigned long long sz) { return kmalloc((size_t)sz); }
void *operator new[](unsigned long long sz) { return kmalloc((size_t)sz); }
void operator delete(void *p) { kfree(p); }
void operator delete(void *p, unsigned long long) { kfree(p); }
void operator delete[](void *p) { kfree(p); }
void operator delete[](void *p, unsigned long long) { kfree(p); }
