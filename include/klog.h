#ifndef ASTRAOS_KLOG_H
#define ASTRAOS_KLOG_H

enum {
    KLOG_INFO = 0,
    KLOG_WARN = 1,
    KLOG_ERROR = 2,
    KLOG_DEBUG = 3
};

#ifdef __cplusplus
extern "C" {
#endif

void klog_printf(int level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* ASTRAOS_KLOG_H */

