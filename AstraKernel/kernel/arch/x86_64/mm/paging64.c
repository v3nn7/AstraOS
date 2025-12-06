#include "memory.h"
#include "vmm.h"

/* Shim kompatybilności: deleguje do VMM, tabele aktywne tworzy vmm_init() */

void paging_init(phys_addr_t kernel_start, phys_addr_t kernel_end) {
    (void)kernel_start;
    (void)kernel_end;
    /* Faktyczna inicjalizacja odbywa się w memory_subsystem_init() -> vmm_init() */
}

int map_page(virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    vmm_map((uint64_t)virt, (uint64_t)phys, flags);
    return 0;
}

void unmap_page(virt_addr_t virt) {
    vmm_unmap((uint64_t)virt);
}

