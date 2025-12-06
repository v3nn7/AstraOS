#include "types.h"
#include "kernel.h"

#define MAX_PROCS 32

typedef enum {
    PROC_UNUSED = 0,
    PROC_NEW,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED
} proc_state_t;

typedef struct {
    int pid;
    proc_state_t state;
    void (*entry)(void *);
    void *arg;
} process_t;

static process_t procs[MAX_PROCS];
static int proc_next_pid = 1;

int create_process(void (*entry)(void *), void *arg) {
    int slot = -1;
    for (int i = 0; i < MAX_PROCS; ++i) {
        if (procs[i].state == PROC_UNUSED) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int pid = proc_next_pid++;
    procs[slot].pid = pid;
    procs[slot].entry = entry;
    procs[slot].arg = arg;
    procs[slot].state = PROC_NEW;

    int sched_pid = scheduler_add_task(entry, arg);
    if (sched_pid < 0) {
        procs[slot].state = PROC_UNUSED;
        return -1;
    }
    procs[slot].state = PROC_READY;
    return pid;
}

int kill_process(int pid) {
    for (int i = 0; i < MAX_PROCS; ++i) {
        if (procs[i].state != PROC_UNUSED && procs[i].pid == pid) {
            procs[i].state = PROC_BLOCKED;
            if (scheduler_kill(pid)) {
                procs[i].state = PROC_UNUSED;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}
