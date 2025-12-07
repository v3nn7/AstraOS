#include "limine.h"

/* ----- Limine 10.x Requests Section Setup ----- */

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(0);

/* Framebuffer request */
__attribute__((used, section(".limine_requests"), aligned(8)))
volatile struct limine_framebuffer_request limine_fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

/* Memory map request */
__attribute__((used, section(".limine_requests"), aligned(8)))
volatile struct limine_memmap_request limine_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

/* HHDM request */
__attribute__((used, section(".limine_requests"), aligned(8)))
volatile struct limine_hhdm_request limine_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

/* Executable address request - needed for kernel physical/virtual base */
__attribute__((used, section(".limine_requests"), aligned(8)))
volatile struct limine_executable_address_request limine_exec_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end[] = LIMINE_REQUESTS_END_MARKER;
