#include "include/msi.h"

void msi_init_config(msi_config_t* cfg) {
    if (!cfg) {
        return;
    }
    int vec = msi_allocator_next_vector();
    cfg->vector = (vec < 0) ? 0 : (uint8_t)vec;
    cfg->masked = false;
}

bool msi_enable(const msi_config_t* cfg) {
    return cfg != 0 && cfg->vector != 0 && cfg->masked == false;
}

bool msi_disable(const msi_config_t* cfg) {
    return cfg != 0;
}
