#pragma once

#include "types.h"

typedef enum {
    DRIVER_GENERIC = 0,
    DRIVER_STORAGE,
    DRIVER_INPUT,
    DRIVER_DISPLAY,
    DRIVER_NET,
    DRIVER_USB
} driver_class_t;

typedef struct driver {
    const char *name;
    driver_class_t cls;
    int (*probe)(void *device);
    int (*init)(void);
    int (*attach)(void *device);
} driver_t;

int driver_manager_init(void);
int driver_register(const driver_t *drv);
const driver_t *driver_find(const char *name);
int driver_attach(const char *name, void *device);

