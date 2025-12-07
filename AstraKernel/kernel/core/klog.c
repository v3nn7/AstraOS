#include "klog.h"
#include "kernel.h"
#include "types.h"
#include "string.h"
#include "drivers.h"

typedef struct {
    klog_level_t level;
    char text[128];
} klog_entry_t;

#define KLOG_CAP 128

static klog_entry_t buffer[KLOG_CAP];
static size_t write_pos = 0;
static klog_level_t current_level = KLOG_INFO;

static int kvsnprintf(char *out, size_t out_sz, const char *fmt, __builtin_va_list ap) {
    size_t pos = 0;
    while (*fmt && pos + 1 < out_sz) {
        if (*fmt != '%') {
            out[pos++] = *fmt++;
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
            uint64_t v = __builtin_va_arg(ap, uint64_t);
            char tmp[32];
            int idx = 0;
            if (v == 0) tmp[idx++] = '0';
            while (v && idx < (int)sizeof(tmp)) {
                uint64_t d = v & 0xF;
                tmp[idx++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
                v >>= 4;
            }
            /* Apply zero padding */
            while (idx < width && idx < (int)sizeof(tmp)) {
                tmp[idx++] = '0';
            }
            while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
            fmt += 3;
            continue;
        }
        
        /* Handle %lu - unsigned long */
        if (*fmt == 'l' && fmt[1] == 'u') {
            uint64_t v = __builtin_va_arg(ap, unsigned long);
            char tmp[32];
            int idx = 0;
            if (v == 0) tmp[idx++] = '0';
            while (v && idx < (int)sizeof(tmp)) {
                tmp[idx++] = (char)('0' + (v % 10));
                v /= 10;
            }
            /* Apply zero padding */
            while (idx < width && idx < (int)sizeof(tmp)) {
                tmp[idx++] = '0';
            }
            while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
            fmt += 2;
            continue;
        }
        
        /* Handle %zu - size_t (unsigned long on x86_64) */
        if (*fmt == 'z' && fmt[1] == 'u') {
            uint64_t v = __builtin_va_arg(ap, unsigned long); /* size_t is unsigned long on x86_64 */
            char tmp[32];
            int idx = 0;
            if (v == 0) tmp[idx++] = '0';
            while (v && idx < (int)sizeof(tmp)) {
                tmp[idx++] = (char)('0' + (v % 10));
                v /= 10;
            }
            /* Apply zero padding */
            while (idx < width && idx < (int)sizeof(tmp)) {
                tmp[idx++] = '0';
            }
            while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
            fmt += 2;
            continue;
        }
        
        /* Handle %zd - ssize_t (signed long on x86_64) */
        if (*fmt == 'z' && fmt[1] == 'd') {
            int64_t v = __builtin_va_arg(ap, long); /* ssize_t is signed long on x86_64 */
            char tmp[32];
            int idx = 0;
            bool neg = v < 0;
            uint64_t u = neg ? (uint64_t)(-v) : (uint64_t)v;
            if (u == 0) tmp[idx++] = '0';
            while (u && idx < (int)sizeof(tmp)) {
                tmp[idx++] = '0' + (u % 10);
                u /= 10;
            }
            /* Apply zero padding */
            while (idx < width && idx < (int)sizeof(tmp)) {
                tmp[idx++] = '0';
            }
            if (neg && pos + 1 < out_sz) out[pos++] = '-';
            while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
            fmt += 2;
            continue;
        }
        
        switch (*fmt) {
            case 'u': {
                unsigned int v = __builtin_va_arg(ap, unsigned int);
                char tmp[32];
                int idx = 0;
                if (v == 0) tmp[idx++] = '0';
                while (v && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = (char)('0' + (v % 10));
                    v /= 10;
                }
                /* Apply zero padding */
                while (idx < width && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = '0';
                }
                while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
                break;
            }
            case 's': {
                const char *s = __builtin_va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s && pos + 1 < out_sz) out[pos++] = *s++;
                break;
            }
            case 'd': {
                int64_t v = __builtin_va_arg(ap, int);
                char tmp[32];
                int idx = 0;
                bool neg = v < 0;
                uint64_t u = neg ? (uint64_t)(-v) : (uint64_t)v;
                if (u == 0) tmp[idx++] = '0';
                while (u && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = '0' + (u % 10);
                    u /= 10;
                }
                /* Apply zero padding */
                while (idx < width && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = '0';
                }
                if (neg && pos + 1 < out_sz) out[pos++] = '-';
                while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
                break;
            }
            case 'x': {
                unsigned int v = __builtin_va_arg(ap, unsigned int);
                char tmp[32];
                int idx = 0;
                if (v == 0) tmp[idx++] = '0';
                while (v && idx < (int)sizeof(tmp)) {
                    uint64_t d = v & 0xF;
                    tmp[idx++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
                    v >>= 4;
                }
                /* Apply zero padding */
                while (idx < width && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = '0';
                }
                while (idx-- && pos + 1 < out_sz) out[pos++] = tmp[idx];
                break;
            }
            case 'p': {
                uint64_t v = (uint64_t)__builtin_va_arg(ap, void *);
                char tmp[32];
                int idx = 0;
                if (pos + 2 < out_sz) { out[pos++] = '0'; out[pos++] = 'x'; }
                if (v == 0) tmp[idx++] = '0';
                while (v && idx < (int)sizeof(tmp)) {
                    uint64_t d = v & 0xF;
                    tmp[idx++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
                    v >>= 4;
                }
                /* Apply zero padding */
                while (idx < width && idx < (int)sizeof(tmp)) {
                    tmp[idx++] = '0';
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

void klog_init(void) {
    write_pos = 0;
    current_level = KLOG_INFO;
    for (size_t i = 0; i < KLOG_CAP; ++i) {
        buffer[i].level = KLOG_TRACE;
        buffer[i].text[0] = 0;
    }
}

void klog_set_level(klog_level_t level) { current_level = level; }
klog_level_t klog_get_level(void) { return current_level; }

const char *klog_level_name(klog_level_t level) {
    switch (level) {
        case KLOG_TRACE: return "TRACE";
        case KLOG_DEBUG: return "DEBUG";
        case KLOG_INFO:  return "INFO";
        case KLOG_WARN:  return "WARN";
        case KLOG_ERROR: return "ERROR";
        case KLOG_FATAL: return "FATAL";
        default: return "UNK";
    }
}

void klog_printf(klog_level_t level, const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char line[128];
    kvsnprintf(line, sizeof(line), fmt, ap);
    __builtin_va_end(ap);

    buffer[write_pos].level = level;
    k_memset(buffer[write_pos].text, 0, sizeof(buffer[write_pos].text));
    memcpy(buffer[write_pos].text, line, sizeof(buffer[write_pos].text) - 1);
    write_pos = (write_pos + 1) % KLOG_CAP;

    if (level >= current_level) {
        printf("[%s] %s\n", klog_level_name(level), line);
    }
}

size_t klog_copy_recent(char *out, size_t max_len) {
    size_t pos = 0;
    for (size_t i = 0; i < KLOG_CAP && pos < max_len; ++i) {
        size_t idx = (write_pos + i) % KLOG_CAP;
        if (!buffer[idx].text[0]) continue;
        const char *lvl = klog_level_name(buffer[idx].level);
        const char *msg = buffer[idx].text;
        for (const char *p = "["; *p && pos + 1 < max_len; ++p) out[pos++] = *p;
        while (*lvl && pos + 1 < max_len) out[pos++] = *lvl++;
        for (const char *p = "] "; *p && pos + 1 < max_len; ++p) out[pos++] = *p;
        while (*msg && pos + 1 < max_len) out[pos++] = *msg++;
        if (pos + 1 < max_len) out[pos++] = '\n';
    }
    if (pos < max_len) out[pos] = 0;
    return pos;
}

