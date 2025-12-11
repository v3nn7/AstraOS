#pragma once

#ifdef __cplusplus
extern "C" {
#endif

static inline int printf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}

#ifdef __cplusplus
}
#endif
