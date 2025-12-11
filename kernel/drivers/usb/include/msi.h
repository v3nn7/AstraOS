#ifndef USB_MSI_H
#define USB_MSI_H

#include <stdbool.h>
#include <stdint.h>
#include "msi_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Simple MSI configuration wrapper. */
typedef struct msi_config {
    uint8_t vector;
    bool masked;
} msi_config_t;

/* Initialize an MSI config with a freshly allocated vector. */
void msi_init_config(msi_config_t* cfg);

/* Enable or disable an MSI vector; returns success. */
bool msi_enable(const msi_config_t* cfg);
bool msi_disable(const msi_config_t* cfg);

/* Basic vector allocator helpers. */
void msi_allocator_reset(void);
int msi_allocator_next_vector(void);
int msi_allocator_used(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_MSI_H */
