#include "ipc.h"
#include "kmalloc.h"

struct ipc_channel {
    uint64_t *buf;
    size_t head;
    size_t tail;
    size_t cap;
};

ipc_channel_t *ipc_channel_create(size_t capacity) {
    if (capacity == 0) capacity = 16;
    ipc_channel_t *ch = (ipc_channel_t *)kcalloc(1, sizeof(ipc_channel_t));
    if (!ch) return NULL;
    ch->buf = (uint64_t *)kcalloc(capacity, sizeof(uint64_t));
    if (!ch->buf) return NULL;
    ch->cap = capacity;
    ch->head = ch->tail = 0;
    return ch;
}

size_t ipc_pending(const ipc_channel_t *ch) {
    if (!ch) return 0;
    if (ch->head >= ch->tail) return ch->head - ch->tail;
    return ch->cap - (ch->tail - ch->head);
}

int ipc_send(ipc_channel_t *ch, uint64_t value) {
    if (!ch) return -1;
    size_t next = (ch->head + 1) % ch->cap;
    if (next == ch->tail) return -1; /* full */
    ch->buf[ch->head] = value;
    ch->head = next;
    return 0;
}

int ipc_recv(ipc_channel_t *ch, uint64_t *value_out) {
    if (!ch || ch->head == ch->tail) return -1;
    uint64_t v = ch->buf[ch->tail];
    ch->tail = (ch->tail + 1) % ch->cap;
    if (value_out) *value_out = v;
    return 0;
}

