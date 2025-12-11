/**
 * Simple MSI vector allocator used by host tests.
 */

#include "pci_msi.h"

static uint32_t g_next_vector = 32;

void msi_allocator_reset(void) {
    g_next_vector = 32;
}

int msi_allocator_next_vector(void) {
    return (int)g_next_vector++;
}
