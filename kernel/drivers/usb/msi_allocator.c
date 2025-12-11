#include "include/msi.h"

/* Trivial global MSI vector allocator; grows linearly from a base. */
static int g_next_vector = 32;

void msi_allocator_reset(void) {
    g_next_vector = 32;
}

int msi_allocator_next_vector(void) {
    if (g_next_vector > 255) {
        return -1;
    }
    return g_next_vector++;
}
