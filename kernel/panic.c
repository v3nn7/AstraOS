#include <kernel/panic.h>

void kernel_panic(const char *message) {
    (void)message;
    /* Panic handler stub; intended to halt the system */
}

