#ifndef USB_MSI_DEFS_H
#define USB_MSI_DEFS_H

#include <stdint.h>
#include <stdbool.h>

/* Minimal MSI/MSI-X capability fields used by the USB stack stubs. */
typedef struct msi_message {
    uint8_t vector;
    uint8_t delivery_mode;
    bool masked;
} msi_message_t;

#endif /* USB_MSI_DEFS_H */
