#pragma once
#include "types.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);

