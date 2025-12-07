#pragma once

#include "types.h"

typedef struct ipc_channel ipc_channel_t;

ipc_channel_t *ipc_channel_create(size_t capacity);
int ipc_send(ipc_channel_t *ch, uint64_t value);
int ipc_recv(ipc_channel_t *ch, uint64_t *value_out);
size_t ipc_pending(const ipc_channel_t *ch);

