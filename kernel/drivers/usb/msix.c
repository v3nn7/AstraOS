#include "include/msix.h"
#include "include/msi.h"

void msix_init_entry(msix_entry_t* entry) {
    if (!entry) {
        return;
    }
    int vec = msi_allocator_next_vector();
    entry->vector = (vec < 0) ? 0 : (uint8_t)vec;
    entry->masked = false;
}

bool msix_enable(const msix_entry_t* entry) {
    return entry != 0 && entry->vector != 0 && entry->masked == false;
}

bool msix_disable(const msix_entry_t* entry) {
    return entry != 0;
}
