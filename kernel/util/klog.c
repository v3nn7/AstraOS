/**
 * Simple kernel logger with ring buffer and serial output.
 *
 * Note: formatting is minimal — varargs are ignored; we log the fmt string.
 */

#include "klog.h"
#include "string.h"
#include "../drivers/serial.hpp"

#define KLOG_MAX_LINES 256
#define KLOG_LINE_LEN  160

static char log_lines[KLOG_MAX_LINES][KLOG_LINE_LEN];
static uint32_t log_head = 0;
static uint32_t log_count = 0;
static klog_level_t g_level = KLOG_INFO;

static void append_line(const char *msg) {
    uint32_t idx = log_head % KLOG_MAX_LINES;
    /* copy at most KLOG_LINE_LEN-1 to ensure null termination */
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

void klog_printf(klog_level_t level, const char *fmt, ...) {
    if (level < g_level) {
        return;
    }

    /* Prefix with level for readability */
    const char *lvl = klog_level_name(level);
    serial_write("[");
    serial_write(lvl);
    serial_write("] ");
    serial_write(fmt);
    serial_write("\r\n");

    append_line(fmt);
}

size_t klog_copy_recent(char *out, size_t max_len) {
    if (!out || max_len == 0 || log_count == 0) {
        return 0;
    }

    size_t written = 0;
    /* Oldest first */
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
