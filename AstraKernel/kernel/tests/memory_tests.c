#include "memory_tests.h"
#include "kernel.h"
#include "memory.h"
#include "kmalloc.h"
#include "dma.h"
#include "vmm.h"
#include "string.h"

static inline uint64_t align_down_u64(uint64_t v, uint64_t a) { return v & ~(a - 1); }

void memory_tests_run(void) {
    printf("MEMTEST: start\n");

    /* PMM page alloc/free */
    void *p = pmm_alloc_page();
    if (!p) {
        printf("MEMTEST: pmm_alloc_page failed\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    pmm_free_page(p);

    /* kmalloc small */
    char *s = (char *)kmalloc(64);
    if (!s) {
        printf("MEMTEST: kmalloc small failed\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    strcpy(s, "kmalloc-ok");

    /* kmalloc large */
    void *l = kmalloc(8192);
    if (!l) {
        printf("MEMTEST: kmalloc large failed\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    k_memset(l, 0xA5, 8192);

    /* DMA allocation */
    uint64_t dma_phys = 0;
    void *dma = dma_alloc(4096, 256, &dma_phys);
    if (!dma || (dma_phys & 0xFF)) {
        printf("MEMTEST: dma_alloc failed\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    uint64_t phys_check = vmm_virt_to_phys((uint64_t)dma);
    if (align_down_u64(phys_check, 256) != align_down_u64(dma_phys, 256)) {
        printf("MEMTEST: dma virt->phys mismatch\n");
        while (1) { __asm__ volatile("cli; hlt"); }
    }
    dma_free(dma, 4096);

    kfree(s);
    kfree(l);
    printf("MEMTEST: ok\n");
}

