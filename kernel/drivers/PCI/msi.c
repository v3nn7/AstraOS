/**
 * Minimal MSI configuration helpers.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t vector;
    bool masked;
} msi_config_t;

void msi_init_config(msi_config_t *cfg) {
    if (!cfg) {
        return;
    }
    cfg->vector = 32;
    cfg->masked = false;
}

bool msi_enable(msi_config_t *cfg) {
    if (!cfg) {
        return false;
    }
    cfg->masked = false;
    if (cfg->vector == 0) {
        cfg->vector = 32;
    }
    return true;
}

bool msi_disable(msi_config_t *cfg) {
    if (!cfg) {
        return false;
    }
    cfg->masked = true;
    return true;
}
