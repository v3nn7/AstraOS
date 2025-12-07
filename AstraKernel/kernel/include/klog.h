#pragma once

#include "types.h"

typedef enum {
    KLOG_TRACE = 0,
    KLOG_DEBUG = 1,
    KLOG_INFO  = 2,
    KLOG_WARN  = 3,
    KLOG_ERROR = 4,
    KLOG_FATAL = 5
} klog_level_t;

#ifdef __cplusplus
extern "C" {
#endif

void klog_init(void);
void klog_set_level(klog_level_t level);
klog_level_t klog_get_level(void);
const char *klog_level_name(klog_level_t level);
void klog_printf(klog_level_t level, const char *fmt, ...);
size_t klog_copy_recent(char *out, size_t max_len);

#ifdef __cplusplus
}
#endif

