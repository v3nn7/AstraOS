#include "panic.h"
#include "interrupts.h"
#include "klog.h"
#include "kernel.h"
#include "types.h"
#include "string.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

__attribute__((weak)) void panic_hook(const char *msg) { (void)msg; }

static int panic_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap) {
    size_t pos = 0;
    while (*fmt && pos + 1 < out_sz) {
        if (*fmt != '%') {
            out[pos++] = *fmt++;
            continue;
        }
        ++fmt;
        switch (*fmt) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s && pos + 1 < out_sz) out[pos++] = *s++;
                break;
            }
            case 'd': {
                int v = va_arg(ap, int);
                char tmp[32];
                int idx = 0;
                bool neg = v < 0;
                uint32_t u = neg ? (uint32_t)(-v) : (uint32_t)v;
                if (u == 0) tmp[idx++] = '0';
                while (u && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = (char)('0' + (u % 10));
                    u /= 10;
                }
                if (neg && pos + 1 < out_sz) out[pos++] = '-';
                while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
                break;
            }
            case 'x':
            case 'p': {
                uint64_t v = (uint64_t)va_arg(ap, void *);
                char tmp[32];
                int idx = 0;
                if (*fmt == 'p' && pos + 2 < out_sz) { out[pos++] = '0'; out[pos++] = 'x'; }
                if (v == 0) tmp[idx++] = '0';
                while (v && idx < (int)sizeof(tmp)) {
                    uint64_t d = v & 0xF;
                    tmp[idx++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
                    v >>= 4;
                }
                while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
                break;
            }
            case '%':
                out[pos++] = '%';
                break;
            default:
                out[pos++] = '?';
                break;
        }
        ++fmt;
    }
    out[pos] = 0;
    return (int)pos;
}

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    interrupts_disable();

    va_list ap;
    va_start(ap, fmt);

    char buf[192];
    panic_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    klog_printf(KLOG_FATAL, "PANIC: %s", buf);
    panic_hook(buf);
    printf("System halted.\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
