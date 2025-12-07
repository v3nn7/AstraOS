#include "initcall.h"
#include "klog.h"
#include "kernel.h"
#include "memory.h" /* for KERNEL_BASE */
#include "string.h"

extern "C" const struct initcall_desc __start_initcalls[];
extern "C" const struct initcall_desc __stop_initcalls[];

#define MAX_INITCALLS 128

static const struct initcall_desc *sorted[MAX_INITCALLS];
static size_t initcall_count = 0;
static bool executed[MAX_INITCALLS];
static bool collected_once = false;

extern "C" int initcall_register(initcall_stage_t stage, initcall_t fn, const char *name) {
    if (!fn || initcall_count >= MAX_INITCALLS) return -1;
    static struct initcall_desc dyn_desc[MAX_INITCALLS];
    dyn_desc[initcall_count].stage = stage;
    dyn_desc[initcall_count].fn = fn;
    dyn_desc[initcall_count].name = name;
    sorted[initcall_count] = &dyn_desc[initcall_count];
    executed[initcall_count] = false;
    ++initcall_count;
    return 0;
}

static void collect_linker_initcalls(void) {
    if (collected_once) {
        printf("initcall: already collected\n");
        return;
    }
    
    printf("initcall: checking linker symbols: __start=%p __stop=%p\n",
           (void *)__start_initcalls, (void *)__stop_initcalls);
    
    /* Check if symbols are in kernel space */
    uintptr_t start_addr = (uintptr_t)__start_initcalls;
    uintptr_t stop_addr = (uintptr_t)__stop_initcalls;
    
    if (start_addr < KERNEL_BASE || stop_addr < KERNEL_BASE) {
        printf("initcall: WARNING - linker symbols not in kernel space\n");
        klog_printf(KLOG_WARN, "initcall: missing linker symbols");
        collected_once = true;
        return;
    }
    
    if (start_addr >= stop_addr) {
        printf("initcall: WARNING - invalid symbol range (start >= stop)\n");
        klog_printf(KLOG_WARN, "initcall: invalid symbol range");
        collected_once = true;
        return;
    }
    
    size_t span = (size_t)(stop_addr - start_addr) / sizeof(struct initcall_desc);
    printf("initcall: span=%zu entries (max=%d)\n", span, MAX_INITCALLS);
    
    if (span > MAX_INITCALLS) {
        printf("initcall: WARNING - span exceeds MAX_INITCALLS, truncating\n");
        span = MAX_INITCALLS;
    }
    
    printf("initcall: collecting %zu entries\n", span);
    for (size_t i = 0; i < span; ++i) {
        sorted[initcall_count++] = &__start_initcalls[i];
    }
    printf("initcall: collected %zu entries, total count=%zu\n", span, initcall_count);
    collected_once = true;
}

extern "C" void initcall_run_all(void) {
    printf("initcall: collecting linker initcalls\n");
    collect_linker_initcalls();
    printf("initcall: collected, count=%u\n", (unsigned)initcall_count);
    klog_printf(KLOG_INFO, "initcall: start count=%u", (unsigned)initcall_count);

    for (int stage = INITCALL_EARLY; stage <= INITCALL_LATE; ++stage) {
        printf("initcall: processing stage %d\n", stage);
        for (size_t i = 0; i < initcall_count; ++i) {
            const struct initcall_desc *d = sorted[i];
            if ((int)d->stage != stage || !d->fn || executed[i]) continue;
            uintptr_t fn_addr = (uintptr_t)d->fn;
            if (fn_addr < KERNEL_BASE) {
                printf("initcall[%d]: %s skipped (bad fn=%p)\n", d->stage, d->name ? d->name : "<noname>", (void *)fn_addr);
                klog_printf(KLOG_WARN, "initcall[%d]: %s skipped (bad fn=%p)", d->stage, d->name ? d->name : "<noname>", (void *)fn_addr);
                executed[i] = true;
                continue;
            }
            printf("initcall[%d]: calling %s (fn=%p)\n", d->stage, d->name ? d->name : "<noname>", (void *)fn_addr);
            int rc = d->fn();
            const char *name = d->name ? d->name : "<noname>";
            printf("initcall[%d]: %s returned %d\n", d->stage, name, rc);
            klog_printf(KLOG_INFO, "initcall[%d]: %s fn=%p -> %d", d->stage, name, (void *)fn_addr, rc);
            executed[i] = true;
        }
    }
    printf("initcall: all done\n");
    klog_printf(KLOG_INFO, "initcall: done");
}


