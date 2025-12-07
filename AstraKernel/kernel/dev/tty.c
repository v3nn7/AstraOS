#include "tty.h"
#include "drivers.h"
#include "klog.h"
#include "string.h"
#include "kernel.h"
#include "interrupts.h"

#define TTY_BUF 512

static char in_buf[TTY_BUF];
static size_t in_head = 0;
static size_t in_tail = 0;

static void tty_putc_fb(char c) {
    if (c == '\n') {
        fb_draw_char(0, fb_height() - 16, ' ', 0xFFFFFFFF, 0x00000000);
    }
    fb_putpixel(0, 0, 0); /* ensure fb is touched so that driver is initialized */
    printf("%c", c);
}

void tty_init(void) {
    in_head = in_tail = 0;
    klog_printf(KLOG_INFO, "tty: initialized");
}

void tty_putc(char c) {
    tty_putc_fb(c);
}

void tty_write(const char *s) {
    while (s && *s) tty_putc(*s++);
}

void tty_feed_char(char c) {
    size_t next = (in_head + 1) % TTY_BUF;
    if (next == in_tail) return; /* drop on overflow */
    in_buf[in_head] = c;
    in_head = next;
}

bool tty_read_char(char *out) {
    if (in_head == in_tail) return false;
    *out = in_buf[in_tail];
    in_tail = (in_tail + 1) % TTY_BUF;
    return true;
}

void tty_poll_input(void) {
    char ch;
    while (keyboard_poll_char(&ch)) {
        tty_feed_char(ch);
    }
}

