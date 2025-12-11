#ifndef USB_MSIX_H
#define USB_MSIX_H

#include <stdbool.h>
#include <stdint.h>
#include "msi_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MSI-X table entry stub used for future controller wiring. */
typedef struct msix_entry {
    uint8_t vector;
    bool masked;
} msix_entry_t;

void msix_init_entry(msix_entry_t* entry);
bool msix_enable(const msix_entry_t* entry);
bool msix_disable(const msix_entry_t* entry);

#ifdef __cplusplus
}
#endif

#endif /* USB_MSIX_H */
