#include "include/msi.h"

/* Trivial global MSI vector allocator; grows linearly from a base. */
static int g_next_vector = 32;
static int g_used = 0;

void msi_allocator_reset(void) {
    g_next_vector = 32;
    g_used = 0;
}

int msi_allocator_next_vector(void) {
    if (g_next_vector > 255) {
        return -1;
    }
    ++g_used;
    return g_next_vector++;
}

int msi_allocator_used(void) {
    return g_used;
}
