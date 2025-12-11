#include "klog.h"
#include "string.h"
#include "../drivers/serial.hpp"
#include <stdarg.h>

#define KLOG_MAX_LINES 256
#define KLOG_LINE_LEN  160

static char log_lines[KLOG_MAX_LINES][KLOG_LINE_LEN];
static uint32_t log_head = 0;
static uint32_t log_count = 0;
static klog_level_t g_level = KLOG_INFO;

static void append_line(const char *msg) {
    uint32_t idx = log_head % KLOG_MAX_LINES;
    size_t i = 0;
    for (; i + 1 < KLOG_LINE_LEN && msg[i]; ++i) {
        log_lines[idx][i] = msg[i];
    }
    log_lines[idx][i] = '\0';

    log_head = (log_head + 1) % KLOG_MAX_LINES;
    if (log_count < KLOG_MAX_LINES) {
        log_count++;
    }
}

static void uint64_to_hex(char* out, uint64_t v) {
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        uint8_t nib = (v >> (60 - 4 * i)) & 0xF;
        out[2 + i] = (nib < 10) ? ('0' + nib) : ('a' + nib - 10);
    }
    out[18] = '\0';
}

static void uint64_to_dec(char* out, uint64_t v) {
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char buf[32];
    int i = 0;
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    int j = 0;
    for (int k = i - 1; k >= 0; k--) {
        out[j++] = buf[k];
    }
    out[j] = '\0';
}

void klog_init(void) {
    serial_init();
    log_head = 0;
    log_count = 0;
    g_level = KLOG_INFO;
}

void klog_set_level(klog_level_t level) {
    g_level = level;
}

klog_level_t klog_get_level(void) {
    return g_level;
}

const char *klog_level_name(klog_level_t level) {
    switch (level) {
        case KLOG_TRACE: return "trace";
        case KLOG_DEBUG: return "debug";
        case KLOG_INFO:  return "info";
        case KLOG_WARN:  return "warn";
        case KLOG_ERROR: return "error";
        case KLOG_FATAL: return "fatal";
        default: return "unknown";
    }
}

static void append_str(char* buf, size_t* pos, const char* s) {
    while (*s && *pos < KLOG_LINE_LEN - 1) {
        buf[(*pos)++] = *s++;
    }
}

void klog_printf(klog_level_t level, const char *fmt, ...) {
    if (level < g_level) {
        return;
    }

    const char *lvl = klog_level_name(level);
    char buf[KLOG_LINE_LEN];
    size_t pos = 0;

    append_str(buf, &pos, "[");
    append_str(buf, &pos, lvl);
    append_str(buf, &pos, "] ");

    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p && pos < KLOG_LINE_LEN - 20; p++) {
        if (*p == '%' && p[1]) {
            p++;
            if (*p == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                char num[16];
                uint64_to_dec(num, val);
                append_str(buf, &pos, num);
            } else if (*p == 'l' && p[1] == 'l' && p[2] == 'u') {
                p += 2;
                unsigned long long val = va_arg(args, unsigned long long);
                char num[32];
                uint64_to_dec(num, val);
                append_str(buf, &pos, num);
            } else if (*p == 'l' && p[1] == 'l' && p[2] == 'x') {
                p += 2;
                unsigned long long val = va_arg(args, unsigned long long);
                char hex[20];
                uint64_to_hex(hex, val);
                append_str(buf, &pos, hex);
            } else if (*p == 'z' && p[1] == 'u') {
                p++;
                size_t val = va_arg(args, size_t);
                char num[32];
                uint64_to_dec(num, val);
                append_str(buf, &pos, num);
            } else if (*p == 'p') {
                void* val = va_arg(args, void*);
                char hex[20];
                uint64_to_hex(hex, (uint64_t)(uintptr_t)val);
                append_str(buf, &pos, hex);
            } else if (*p == 'd') {
                int val = va_arg(args, int);
                if (val < 0) {
                    append_str(buf, &pos, "-");
                    val = -val;
                }
                char num[16];
                uint64_to_dec(num, val);
                append_str(buf, &pos, num);
            } else if (*p == 's') {
                const char* s = va_arg(args, const char*);
                append_str(buf, &pos, s);
            } else if (*p == '%') {
                append_str(buf, &pos, "%");
            } else {
                append_str(buf, &pos, "%");
                if (pos < KLOG_LINE_LEN - 1) buf[pos++] = *p;
            }
        } else {
            if (pos < KLOG_LINE_LEN - 1) buf[pos++] = *p;
        }
    }

    va_end(args);
    buf[pos] = '\0';

    serial_write(buf);
    serial_write("\r\n");

    append_line(buf);
}

size_t klog_copy_recent(char *out, size_t max_len) {
    if (!out || max_len == 0 || log_count == 0) {
        return 0;
    }

    size_t written = 0;
    uint32_t start = (log_head + KLOG_MAX_LINES - log_count) % KLOG_MAX_LINES;
    for (uint32_t i = 0; i < log_count && written < max_len - 1; ++i) {
        const char *line = log_lines[(start + i) % KLOG_MAX_LINES];
        for (size_t j = 0; line[j] && written < max_len - 1; ++j) {
            out[written++] = line[j];
        }
        if (written < max_len - 1) {
            out[written++] = '\n';
        }
    }
    out[written] = '\0';
    return written;
}