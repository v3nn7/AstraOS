#pragma once

#include "types.h"

typedef int (*initcall_t)(void);

typedef enum {
    INITCALL_EARLY = 0,
    INITCALL_CORE  = 1,
    INITCALL_SUBSYS = 2,
    INITCALL_DRIVER = 3,
    INITCALL_LATE   = 4
} initcall_stage_t;

#ifdef __cplusplus
extern "C" {
#endif

int initcall_register(initcall_stage_t stage, initcall_t fn, const char *name);
void initcall_run_all(void);

#ifdef __cplusplus
}
#endif

struct initcall_desc {
    initcall_stage_t stage;
    initcall_t fn;
    const char *name;
};

#define INITCALL_DEFINE(stage, fn) \
    static const struct initcall_desc __initcall_##fn __attribute__((used, section(".initcalls"))) = { stage, fn, #fn }

