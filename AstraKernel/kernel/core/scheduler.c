#include "types.h"
#include "kernel.h"
#include "interrupts.h"

#define MAX_TASKS 32
#define STACK_SIZE 4096

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
} task_state_t;

typedef struct {
    uint64_t rsp;
    void (*entry)(void *);
    void *arg;
    task_state_t state;
    int pid;
    uint8_t stack[STACK_SIZE];
} task_t;

static task_t tasks[MAX_TASKS];
static int current_idx = -1;
static int pid_next = 1;
static volatile bool need_resched;

static void task_trampoline(void);
static void halt_forever(void) {
    for (;;) __asm__ volatile("cli; hlt");
}

void scheduler_init(void) {
    for (int i = 0; i < MAX_TASKS; ++i) tasks[i].state = TASK_UNUSED;
    need_resched = false;
}

static int pick_next(void) {
    int start = (current_idx >= 0) ? current_idx + 1 : 0;
    for (int i = 0; i < MAX_TASKS; ++i) {
        int idx = (start + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) return idx;
    }
    return -1;
}

int scheduler_add_task(void (*entry)(void *), void *arg) {
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_UNUSED) { slot = i; break; }
    }
    if (slot < 0) return -1;

    task_t *t = &tasks[slot];
    t->entry = entry;
    t->arg = arg;
    t->state = TASK_READY;
    t->pid = pid_next++;

    uint64_t *sp = (uint64_t *)(t->stack + STACK_SIZE);
    *(--sp) = (uint64_t)task_trampoline;
    t->rsp = (uint64_t)sp;
    return t->pid;
}

void scheduler_tick(interrupt_frame_t *frame) {
    (void)frame;
    need_resched = true;
}

void scheduler_yield(void) {
    if (current_idx < 0) {
        int next = pick_next();
        if (next < 0) { need_resched = false; return; }
        current_idx = next;
        tasks[next].state = TASK_RUNNING;
        __asm__ volatile(
            "mov %0, %%rsp\n"
            "ret\n"
            :
            : "m"(tasks[next].rsp)
            : "memory");
    }

    if (!need_resched) return;
    int next = pick_next();
    if (next < 0 || next == current_idx) { need_resched = false; return; }

    task_t *current = &tasks[current_idx];
    task_t *target = &tasks[next];

    current->state = TASK_READY;
    target->state = TASK_RUNNING;

    __asm__ volatile("mov %%rsp, %0" : "=m"(current->rsp));
    current_idx = next;
    need_resched = false;
    __asm__ volatile(
        "mov %0, %%rsp\n"
        "ret\n"
        :
        : "m"(target->rsp)
        : "memory");
}

bool scheduler_kill(int pid) {
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].pid == pid) {
            tasks[i].state = TASK_UNUSED;
            if (i == current_idx) {
                need_resched = true;
                scheduler_yield();
            }
            return true;
        }
    }
    return false;
}

static void task_trampoline(void) {
    int idx = current_idx;
    if (idx < 0 || idx >= MAX_TASKS) halt_forever();
    task_t *t = &tasks[idx];
    t->entry(t->arg);
    t->state = TASK_UNUSED;
    need_resched = true;
    scheduler_yield();
    halt_forever();
}
