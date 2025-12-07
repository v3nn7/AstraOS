#include "types.h"
#include "drivers.h"
#include "interrupts.h"
#include "kernel.h"
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)

static uint32_t cursor_x, cursor_y;
static const uint32_t fg_color = 0xFFFFFFFF;
static const uint32_t bg_color = 0x00000000;
static bool serial_ready;

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
    serial_ready = true;
}

static void serial_putc(char c) {
    if (!serial_ready) serial_init();
    while (!(inb(0x3F8 + 5) & 0x20)) { }
    outb(0x3F8, (uint8_t)c);
}

static void fb_putc(char c) {
    if (!fb_width()) {
        return;
    }
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += 8;
        return;
    }
    fb_draw_char(cursor_x, cursor_y, c, fg_color, bg_color);
    cursor_x += 8;
    if (cursor_x + 8 >= fb_width()) {
        cursor_x = 0;
        cursor_y += 8;
    }
}

static void puts_str(const char *s) {
    while (*s) {
        fb_putc(*s);
        serial_putc(*s);
        ++s;
    }
}

static size_t strlen_local(const char *s) {
    size_t n = 0;
    while (s && *s++) ++n;
    return n;
}

static void utoa_base(uint64_t v, char *buf, int base) {
    char tmp[32];
    const char *digits = "0123456789abcdef";
    int i = 0;
    if (v == 0) tmp[i++] = '0';
    while (v > 0) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    int pos = 0;
    while (i--) buf[pos++] = tmp[i];
    buf[pos] = 0;
}

static void itoa_signed(int64_t v, char *buf) {
    if (v < 0) {
        *buf++ = '-';
        utoa_base((uint64_t)(-v), buf, 10);
    } else {
        utoa_base((uint64_t)v, buf, 10);
    }
}

static void utoa_base_padded(uint64_t v, char *buf, int base, int width, bool zero_pad) {
    char tmp[64];
    const char *digits = "0123456789abcdef";
    int i = 0;
    if (v == 0) tmp[i++] = '0';
    while (v > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    /* Apply padding */
    if (zero_pad) {
        while (i < width && i < (int)sizeof(tmp)) {
            tmp[i++] = '0';
        }
    }
    int pos = 0;
    while (i--) buf[pos++] = tmp[i];
    buf[pos] = 0;
}

int printf(const char *fmt, ...) {
    char numbuf[64];
    va_list ap;
    va_start(ap, fmt);
    int written = 0;
    while (*fmt) {
        if (*fmt != '%') {
            fb_putc(*fmt);
            serial_putc(*fmt);
            ++fmt; ++written;
            continue;
        }
        ++fmt;
        
        /* Parse width specifier and zero padding (e.g., %04x) */
        int width = 0;
        bool zero_pad = false;
        if (*fmt == '0') {
            zero_pad = true;
            ++fmt;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                ++fmt;
            }
        } else if (*fmt >= '1' && *fmt <= '9') {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                ++fmt;
            }
        }
        
        /* Handle %llx - long long hex */
        if (*fmt == 'l' && fmt[1] == 'l' && fmt[2] == 'x') {
            uint64_t val = va_arg(ap, uint64_t);
            utoa_base_padded(val, numbuf, 16, width, zero_pad);
            puts_str(numbuf);
            written += (int)strlen_local(numbuf);
            fmt += 3;
            continue;
        }
        
        /* Handle %lu - unsigned long */
        if (*fmt == 'l' && fmt[1] == 'u') {
            uint64_t val = va_arg(ap, unsigned long);
            utoa_base_padded(val, numbuf, 10, width, zero_pad);
            puts_str(numbuf);
            written += (int)strlen_local(numbuf);
            fmt += 2;
            continue;
        }
        
        /* Handle %zu - size_t (unsigned long on x86_64) */
        if (*fmt == 'z' && fmt[1] == 'u') {
            uint64_t val = va_arg(ap, unsigned long); /* size_t is unsigned long on x86_64 */
            utoa_base_padded(val, numbuf, 10, width, zero_pad);
            puts_str(numbuf);
            written += (int)strlen_local(numbuf);
            fmt += 2;
            continue;
        }
        
        /* Handle %zd - ssize_t (signed long on x86_64) */
        if (*fmt == 'z' && fmt[1] == 'd') {
            int64_t val = va_arg(ap, long); /* ssize_t is signed long on x86_64 */
            itoa_signed(val, numbuf);
            puts_str(numbuf);
            written += (int)strlen_local(numbuf);
            fmt += 2;
            continue;
        }
        
        switch (*fmt) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                puts_str(s);
                written += (int)strlen_local(s);
                break;
            }
            case 'd': {
                int val = va_arg(ap, int);
                itoa_signed(val, numbuf);
                puts_str(numbuf);
                written += (int)strlen_local(numbuf);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(ap, unsigned int);
                utoa_base_padded(val, numbuf, 10, width, zero_pad);
                puts_str(numbuf);
                written += (int)strlen_local(numbuf);
                break;
            }
            case 'x': {
                unsigned int val = va_arg(ap, unsigned int);
                utoa_base_padded(val, numbuf, 16, width, zero_pad);
                puts_str(numbuf);
                written += (int)strlen_local(numbuf);
                break;
            }
            case 'p': {
                uint64_t val = (uint64_t)va_arg(ap, void *);
                numbuf[0] = '0'; numbuf[1] = 'x';
                utoa_base_padded(val, numbuf + 2, 16, width, zero_pad);
                puts_str(numbuf);
                written += (int)strlen_local(numbuf);
                break;
            }
            case '%':
                fb_putc('%');
                serial_putc('%');
                ++written;
                break;
            default:
                fb_putc('?');
                serial_putc('?');
                ++written;
                break;
        }
        ++fmt;
    }
    va_end(ap);
    return written;
}
