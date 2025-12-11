/**
 * Minimal ACPI parser for LAPIC/IOAPIC/HPET/MCFG.
 * Falls back to PC defaults if tables are missing.
 */

#include "acpi.h"
#include "string.h"
#include "klog.h"

extern uint64_t pmm_hhdm_offset;

static bool acpi_inited = false;
static uint64_t lapic_phys = 0xFEE00000ULL;
static uint64_t ioapic_phys = 0xFEC00000ULL;
static uint32_t ioapic_gsi_base = 0;
static uint64_t hpet_phys = 0xFED00000ULL;
static uint64_t ecam_phys = 0;
static uint8_t ecam_bus_start = 0;
static uint8_t ecam_bus_end = 0;
static struct {
    uint8_t src_irq;
    uint32_t gsi;
    uint16_t flags;
} iso[16];
static uint8_t iso_count = 0;
static struct {
    uint8_t acpi_cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} lapic_entries[32];
static uint8_t lapic_count = 0;

/* Structures */
struct __attribute__((packed)) Rsdp {
    char sig[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t rev;
    uint32_t rsdt;
    uint32_t length;
    uint64_t xsdt;
    uint8_t ext_checksum;
    uint8_t reserved[3];
};

struct __attribute__((packed)) Sdt {
    char sig[4];
    uint32_t length;
    uint8_t rev;
    uint8_t checksum;
    char oemid[6];
    char oemtable[8];
    uint32_t oemrev;
    uint32_t creator;
    uint32_t creator_rev;
};

struct __attribute__((packed)) Madt {
    struct Sdt h;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t entries[];
};

struct __attribute__((packed)) MadtIoApic {
    uint8_t type;
    uint8_t len;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t gsi_base;
};

struct __attribute__((packed)) MadtLapicOverride {
    uint8_t type;
    uint8_t len;
    uint16_t flags;
    uint32_t lapic_addr;
};

struct __attribute__((packed)) MadtLapic {
    uint8_t type;
    uint8_t len;
    uint8_t acpi_cpu_id;
    uint8_t apic_id;
    uint32_t flags;
};

struct __attribute__((packed)) MadtIso {
    uint8_t type;
    uint8_t len;
    uint8_t bus;
    uint8_t src;
    uint32_t gsi;
    uint16_t flags;
};

struct __attribute__((packed)) HpetTable {
    struct Sdt h;
    uint32_t id;
    uint64_t base;
    uint8_t num;
    uint16_t min_tick;
    uint8_t page_prot;
};

struct __attribute__((packed)) McfgSeg {
    uint64_t base;
    uint16_t seg;
    uint8_t start;
    uint8_t end;
    uint32_t reserved;
};

struct __attribute__((packed)) Mcfg {
    struct Sdt h;
    uint64_t reserved;
    struct McfgSeg entry[];
};

static uint8_t checksum(const uint8_t* p, uint32_t len) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) sum += p[i];
    return sum;
}

static void* phys_to_ptr(uint64_t phys) {
    /* UEFI typically identity maps low memory where ACPI tables live. */
    return (void*)(uintptr_t)phys;
}

static void parse_madt(struct Madt* m) {
    lapic_phys = m->lapic_addr;
    uint32_t offset = sizeof(struct Madt);
    while (offset + 2 <= m->h.length) {
        uint8_t* ent = m->entries + (offset - sizeof(struct Madt));
        uint8_t type = ent[0];
        uint8_t len = ent[1];
        if (len < 2) break;
        if (type == 0 && len >= sizeof(struct MadtLapic) && lapic_count < 32) { // LAPIC
            struct MadtLapic* lap = (struct MadtLapic*)ent;
            lapic_entries[lapic_count].acpi_cpu_id = lap->acpi_cpu_id;
            lapic_entries[lapic_count].apic_id = lap->apic_id;
            lapic_entries[lapic_count].flags = lap->flags;
            lapic_count++;
        } else if (type == 1 && len >= sizeof(struct MadtIoApic)) { // IOAPIC
            struct MadtIoApic* io = (struct MadtIoApic*)ent;
            ioapic_phys = io->addr;
            ioapic_gsi_base = io->gsi_base;
        } else if (type == 5 && len >= sizeof(struct MadtLapicOverride)) { // LAPIC override
            struct MadtLapicOverride* lo = (struct MadtLapicOverride*)ent;
            lapic_phys = lo->lapic_addr;
        } else if (type == 2 && len >= sizeof(struct MadtIso) && iso_count < 16) { // ISO
            struct MadtIso* is = (struct MadtIso*)ent;
            iso[iso_count].src_irq = is->src;
            iso[iso_count].gsi = is->gsi;
            iso[iso_count].flags = is->flags;
            iso_count++;
        }
        offset += len;
    }
}

static void parse_hpet(struct HpetTable* h) {
    hpet_phys = h->base;
}

static void parse_mcfg(struct Mcfg* m) {
    /* take first segment */
    if (m->h.length < sizeof(struct Mcfg)) return;
    struct McfgSeg *e = &m->entry[0];
    ecam_phys = e->base;
    ecam_bus_start = e->start;
    ecam_bus_end = e->end;
}

static void scan_tables(uint64_t sdt_addr, bool xsdt) {
    struct Sdt* sdt = (struct Sdt*)phys_to_ptr(sdt_addr);
    if (!sdt || checksum((uint8_t*)sdt, sdt->length) != 0) return;
    uint32_t count = (sdt->length - sizeof(struct Sdt)) / (xsdt ? 8 : 4);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t entry = xsdt ? ((uint64_t*) (sdt + 1))[i] : ((uint32_t*) (sdt + 1))[i];
        struct Sdt* h = (struct Sdt*)phys_to_ptr(entry);
        if (!h) continue;
        if (checksum((uint8_t*)h, h->length) != 0) continue;
        if (memcmp(h->sig, "APIC", 4) == 0) {
            parse_madt((struct Madt*)h);
            klog_printf(KLOG_INFO, "acpi: MADT parsed lapic=0x%llx ioapic=0x%llx gsi=%u",
                        (unsigned long long)lapic_phys, (unsigned long long)ioapic_phys, ioapic_gsi_base);
        } else if (memcmp(h->sig, "HPET", 4) == 0) {
            parse_hpet((struct HpetTable*)h);
            klog_printf(KLOG_INFO, "acpi: HPET parsed base=0x%llx", (unsigned long long)hpet_phys);
        } else if (memcmp(h->sig, "MCFG", 4) == 0) {
            parse_mcfg((struct Mcfg*)h);
            klog_printf(KLOG_INFO, "acpi: MCFG parsed ecam=0x%llx bus=%u-%u",
                        (unsigned long long)ecam_phys, ecam_bus_start, ecam_bus_end);
        }
    }
}

static void find_rsdp_and_parse(void) {
    /* Search EBDA and 0xE0000-0xFFFFF */
    for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        struct Rsdp* r = (struct Rsdp*)phys_to_ptr(addr);
        if (!r) continue;
        if (memcmp(r->sig, "RSD PTR ", 8) != 0) continue;
        if (checksum((uint8_t*)r, 20) != 0) continue;
        uint64_t sdt = 0;
        bool xsdt = false;
        if (r->rev >= 2 && r->xsdt) {
            if (checksum((uint8_t*)r, r->length) != 0) continue;
            sdt = r->xsdt;
            xsdt = true;
        } else {
            sdt = r->rsdt;
            xsdt = false;
        }
        scan_tables(sdt, xsdt);
        return;
    }
}

void acpi_init(void) {
    if (acpi_inited) return;
    acpi_inited = true;
    find_rsdp_and_parse();
}

uint64_t acpi_get_lapic_address(void) {
    acpi_init();
    return lapic_phys;
}

uint64_t acpi_get_hpet_address(void) {
    acpi_init();
    return hpet_phys;
}

void acpi_get_ioapic(uint64_t *phys, uint32_t *gsi_base) {
    acpi_init();
    if (phys) *phys = ioapic_phys;
    if (gsi_base) *gsi_base = ioapic_gsi_base;
}

void acpi_get_pcie_ecam(uint64_t *phys_base, uint8_t *start_bus, uint8_t *end_bus) {
    acpi_init();
    if (phys_base) *phys_base = ecam_phys;
    if (start_bus) *start_bus = ecam_bus_start;
    if (end_bus) *end_bus = ecam_bus_end;
}

bool acpi_get_isa_irq_override(uint8_t src_irq, uint32_t *gsi_out, uint16_t *flags_out) {
    acpi_init();
    for (uint8_t i = 0; i < iso_count; ++i) {
        if (iso[i].src_irq == src_irq) {
            if (gsi_out) *gsi_out = iso[i].gsi;
            if (flags_out) *flags_out = iso[i].flags;
            return true;
        }
    }
    return false;
}

uint8_t acpi_get_lapic_count(void) {
    acpi_init();
    return lapic_count;
}

bool acpi_get_lapic_entry(uint8_t index, uint8_t *acpi_cpu_id, uint8_t *apic_id, uint32_t *flags) {
    acpi_init();
    if (index >= lapic_count) return false;
    if (acpi_cpu_id) *acpi_cpu_id = lapic_entries[index].acpi_cpu_id;
    if (apic_id) *apic_id = lapic_entries[index].apic_id;
    if (flags) *flags = lapic_entries[index].flags;
    return true;
}
