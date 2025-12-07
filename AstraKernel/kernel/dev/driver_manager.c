#include "driver_manager.h"
#include "klog.h"
#include "string.h"
#include "initcall.h"

#define MAX_DRIVERS 32

static const driver_t *drivers[MAX_DRIVERS];
static size_t driver_count = 0;

int driver_manager_init(void) {
    driver_count = 0;
    klog_printf(KLOG_INFO, "driver_manager: ready (max=%d)", MAX_DRIVERS);
    return 0;
}

int driver_register(const driver_t *drv) {
    if (!drv || !drv->name) return -1;
    if (driver_count >= MAX_DRIVERS) return -1;
    drivers[driver_count++] = drv;
    klog_printf(KLOG_DEBUG, "driver_manager: registered %s", drv->name);
    if (drv->init) drv->init();
    return 0;
}

const driver_t *driver_find(const char *name) {
    for (size_t i = 0; i < driver_count; ++i) {
        if (strcmp(drivers[i]->name, name) == 0) return drivers[i];
    }
    return NULL;
}

int driver_attach(const char *name, void *device) {
    const driver_t *d = driver_find(name);
    if (!d || !d->attach) return -1;
    return d->attach(device);
}

INITCALL_DEFINE(INITCALL_CORE, driver_manager_init);

