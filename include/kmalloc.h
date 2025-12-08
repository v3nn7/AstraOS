#ifndef ASTRAOS_KMALLOC_H
#define ASTRAOS_KMALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *kmalloc(size_t size);
void kfree(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* ASTRAOS_KMALLOC_H */

