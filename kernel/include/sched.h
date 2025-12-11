#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sched_init(void);
void sched_tick(void);
void sched_sleep_ms(uint64_t ms);
void sched_yield(void);

typedef struct thread {
    uint64_t id;
    uint64_t state;
    void *stack;
} thread_t;

#ifdef __cplusplus
}
#endif

