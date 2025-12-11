/*
 * AstraOS Source-Available License (ASAL v2.1 – Restricted Forking)
 * Copyright (c) 2025 Krystian "v3nn7"
 * All rights reserved.
 *
 * Permission is granted to VIEW the source code of AstraOS (“Software”) for
 * personal, educational, and non-commercial study purposes only.
 *
 * Unless explicit WRITTEN permission is granted by Krystian “v3nn7” R.,
 * the following actions are STRICTLY prohibited:
 *
 * 1. Forking, copying, or cloning the Software or any portion of it.
 * 2. Modifying, compiling, building, or distributing the Software.
 * 3. Using the Software as part of another operating system, kernel,
 *    bootloader, driver, library, or any other project.
 * 4. Any form of commercial use, including selling, licensing, donations,
 *    monetization, hosted services, paid support, sponsorships, or ads.
 * 5. Redistributing the Software or derivative works, modified or unmodified.
 * 6. Uploading the Software to other repositories, mirrors, or datasets.
 * 7. Using the Software in AI training or machine learning datasets.
 *
 * === CONDITIONAL FORK PERMISSION ===
 * Forking, modifying, compiling, or deriving from the Software is permitted ONLY if:
 * - Explicit written approval is granted personally by Krystian "v3nn7" R., AND
 * - The derivative work remains strictly NON-COMMERCIAL.
 *
 * Any breach of these terms voids all permissions immediately and permanently.
 *
 * === LIABILITY ===
 * The Software is provided “AS IS”, without warranty of any kind.
 * The author is not liable for any damages resulting from use or misuse.
 *
 * === CONTACT FOR PERMISSIONS ===
 * All permission requests must be submitted EXCLUSIVELY via GitHub:
 * - GitHub Issues on the official AstraOS repository
 * - GitHub Discussions
 * - Direct GitHub contact: https://github.com/v3nn7
 *
 * Requests via any other medium are automatically rejected.
 *
 * ================================================================================================================
 * Basic AHCI driver: locate the first AHCI controller, bring up one active
 * port, and support synchronous READ DMA EXT transfers. Under HOST_TEST a
 * deterministic in-memory disk backs reads so unit tests can exercise the
 * interface without hardware.
 */

#include "ahci.h"
#include "dma.h"
#include "klog.h"
#include "memory.h"
#include "mmio.h"
#include <drivers/PCI/pci_config.h>
#include <stdbool.h>
#include <string.h>

#define AHCI_SECTOR_SIZE 512u
#define AHCI_MAX_PORTS   32
#define AHCI_CMD_SLOT    0
#define AHCI_BAR5_OFFSET 0x24

/* HBA register offsets */
#define AHCI_REG_CAP 0x00
#define AHCI_REG_GHC 0x04
#define AHCI_REG_IS  0x08
#define AHCI_REG_PI  0x0C

/* Port register offsets (relative to port base) */
#define AHCI_P_CLB  0x00
#define AHCI_P_CLBU 0x04
#define AHCI_P_FB   0x08
#define AHCI_P_FBU  0x0C
#define AHCI_P_IS   0x10
#define AHCI_P_IE   0x14
#define AHCI_P_CMD  0x18
#define AHCI_P_TFD  0x20
#define AHCI_P_SIG  0x24
#define AHCI_P_SSTS 0x28
#define AHCI_P_SCTL 0x2C
#define AHCI_P_SERR 0x30
#define AHCI_P_SACT 0x34
#define AHCI_P_CI   0x38

/* Bit helpers */
#define AHCI_GHC_AE        (1u << 31)
#define AHCI_P_CMD_ST      (1u << 0)
#define AHCI_P_CMD_SUD     (1u << 1)
#define AHCI_P_CMD_FRE     (1u << 4)
#define AHCI_P_CMD_FR      (1u << 14)
#define AHCI_P_CMD_CR      (1u << 15)
#define AHCI_TFD_BSY       0x80
#define AHCI_TFD_DRQ       0x08
#define AHCI_TFD_ERR       0x01
#define AHCI_P_IS_TFES     (1u << 30)

#define AHCI_SSTS_DET_MASK 0x0Fu
#define AHCI_SSTS_IPM_MASK 0x0F00u
#define AHCI_SSTS_IPM_SHIFT 8
#define AHCI_SSTS_DET_PRESENT 0x3u
#define AHCI_SSTS_IPM_ACTIVE  0x1u

#define AHCI_PORT_BASE  0x100
#define AHCI_PORT_STRIDE 0x80

#define AHCI_MAX_PRDT_BYTES 0x400000u /* 4 MiB per PRDT entry */

typedef struct __attribute__((packed)) {
    uint8_t fis_type;
    uint8_t pmport_c;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t reserved[4];
} fis_reg_h2d_t;

typedef struct __attribute__((packed)) {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc_i;
} hba_prdt_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    hba_prdt_entry_t prdt[1];
} hba_cmd_table_t;

typedef struct __attribute__((packed)) {
    uint8_t cfl;
    uint8_t flags;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} hba_cmd_header_t;

typedef struct {
    void *clb;
    phys_addr_t clb_phys;
    void *fb;
    phys_addr_t fb_phys;
    hba_cmd_table_t *table;
    phys_addr_t table_phys;
    bool allocated;
} ahci_port_mem_t;

static uintptr_t g_hba_phys = 0;
static uint8_t g_hba_bus = 0;
static uint8_t g_hba_slot = 0;
static uint8_t g_hba_func = 0;
static uint32_t g_hba_ports = 0;
static int g_active_port = -1;
static bool g_inited = false;
static ahci_port_mem_t g_ports[AHCI_MAX_PORTS];

#ifdef HOST_TEST
#define AHCI_FAKE_SECTORS 16u
static bool g_fake_mode = false;
static uint8_t g_fake_disk[AHCI_FAKE_SECTORS * AHCI_SECTOR_SIZE];
#endif

static inline uintptr_t hba_reg(uint32_t off) {
    return g_hba_phys + off;
}

static inline uintptr_t port_reg(int port, uint32_t off) {
    return g_hba_phys + AHCI_PORT_BASE + (uintptr_t)port * AHCI_PORT_STRIDE + off;
}

static inline uint32_t hba_read32(uint32_t off) {
    return mmio_read32((volatile uint32_t *)hba_reg(off));
}

static inline void hba_write32(uint32_t off, uint32_t val) {
    mmio_write32((volatile uint32_t *)hba_reg(off), val);
}

static inline uint32_t port_read32(int port, uint32_t off) {
    return mmio_read32((volatile uint32_t *)port_reg(port, off));
}

static inline void port_write32(int port, uint32_t off, uint32_t val) {
    mmio_write32((volatile uint32_t *)port_reg(port, off), val);
}

static bool port_wait_bits_clear(int port, uint32_t off, uint32_t mask, uint32_t spins) {
    for (uint32_t i = 0; i < spins; ++i) {
        if ((port_read32(port, off) & mask) == 0) return true;
    }
    return false;
}

static bool port_wait_ready(int port) {
    const uint32_t mask = AHCI_TFD_BSY | AHCI_TFD_DRQ;
    for (uint32_t i = 0; i < 100000; ++i) {
        if ((port_read32(port, AHCI_P_TFD) & mask) == 0) return true;
    }
    return false;
}

static void port_stop(int port) {
    uint32_t cmd = port_read32(port, AHCI_P_CMD);
    if (cmd & AHCI_P_CMD_ST) {
        cmd &= ~AHCI_P_CMD_ST;
        port_write32(port, AHCI_P_CMD, cmd);
        (void)port_wait_bits_clear(port, AHCI_P_CMD, AHCI_P_CMD_CR, 100000);
    }
    cmd = port_read32(port, AHCI_P_CMD);
    if (cmd & AHCI_P_CMD_FRE) {
        cmd &= ~AHCI_P_CMD_FRE;
        port_write32(port, AHCI_P_CMD, cmd);
        (void)port_wait_bits_clear(port, AHCI_P_CMD, AHCI_P_CMD_FR, 100000);
    }
}

static void port_start(int port) {
    uint32_t cmd = port_read32(port, AHCI_P_CMD);
    cmd |= AHCI_P_CMD_FRE | AHCI_P_CMD_SUD | AHCI_P_CMD_ST;
    port_write32(port, AHCI_P_CMD, cmd);
}

static bool port_has_device(int port) {
    uint32_t ssts = port_read32(port, AHCI_P_SSTS);
    uint32_t det = ssts & AHCI_SSTS_DET_MASK;
    uint32_t ipm = (ssts & AHCI_SSTS_IPM_MASK) >> AHCI_SSTS_IPM_SHIFT;
    return det == AHCI_SSTS_DET_PRESENT && ipm >= AHCI_SSTS_IPM_ACTIVE;
}

static bool alloc_port_mem(ahci_port_mem_t *pmem) {
    if (pmem->allocated) return true;
    pmem->clb = dma_alloc(1024, 1024, &pmem->clb_phys);
    pmem->fb = dma_alloc(256, 256, &pmem->fb_phys);
    pmem->table = (hba_cmd_table_t *)dma_alloc(sizeof(hba_cmd_table_t), 128, &pmem->table_phys);
    if (!pmem->clb || !pmem->fb || !pmem->table) return false;
    pmem->allocated = true;
    return true;
}

static int configure_port(int port) {
    ahci_port_mem_t *pm = &g_ports[port];
    if (!alloc_port_mem(pm)) return -1;

    port_stop(port);
    port_write32(port, AHCI_P_SERR, 0xFFFFFFFFu);
    port_write32(port, AHCI_P_IS, 0xFFFFFFFFu);

    memset(pm->clb, 0, 1024);
    memset(pm->fb, 0, 256);
    memset(pm->table, 0, sizeof(hba_cmd_table_t));

    port_write32(port, AHCI_P_CLB, (uint32_t)(pm->clb_phys & 0xFFFFFFFFu));
    port_write32(port, AHCI_P_CLBU, (uint32_t)(pm->clb_phys >> 32));
    port_write32(port, AHCI_P_FB, (uint32_t)(pm->fb_phys & 0xFFFFFFFFu));
    port_write32(port, AHCI_P_FBU, (uint32_t)(pm->fb_phys >> 32));

    hba_cmd_header_t *hdr = (hba_cmd_header_t *)pm->clb;
    hdr[AHCI_CMD_SLOT].cfl = sizeof(fis_reg_h2d_t) / 4;
    hdr[AHCI_CMD_SLOT].prdtl = 1;
    hdr[AHCI_CMD_SLOT].ctba = (uint32_t)(pm->table_phys & 0xFFFFFFFFu);
    hdr[AHCI_CMD_SLOT].ctbau = (uint32_t)(pm->table_phys >> 32);

    port_start(port);
    return 0;
}

static bool find_controller(uintptr_t *bar_phys_out) {
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_cfg_read32(bus, dev, func, 0x00);
                uint16_t vendor = (uint16_t)(id & 0xFFFFu);
                if (vendor == 0xFFFFu || vendor == 0x0000u) {
                    if (func == 0) break;
                    continue;
                }
                uint32_t class_code = pci_cfg_read32(bus, dev, func, 0x08);
                uint8_t base = (class_code >> 24) & 0xFFu;
                uint8_t sub = (class_code >> 16) & 0xFFu;
                uint8_t prog = (class_code >> 8) & 0xFFu;
                if (base == 0x01 && sub == 0x06 && prog == 0x01) {
                    bool is64 = false;
                    uint64_t bar = pci_cfg_read_bar(bus, dev, func, AHCI_BAR5_OFFSET, &is64);
                    if (bar == 0 || is64) continue; /* prefer 32-bit BAR for now */
                    g_hba_bus = (uint8_t)bus;
                    g_hba_slot = dev;
                    g_hba_func = func;
                    *bar_phys_out = (uintptr_t)bar;
                    pci_enable_busmaster(bus, dev, func);
                    return true;
                }
                if (func == 0 && (pci_cfg_read32(bus, dev, func, 0x0C) & 0x00800000u) == 0) {
                    break;
                }
            }
        }
    }
    return false;
}

static int issue_read(int port, uint64_t lba, void *buf, size_t sectors) {
    ahci_port_mem_t *pm = &g_ports[port];
    hba_cmd_header_t *hdr = (hba_cmd_header_t *)pm->clb;
    hba_cmd_table_t *tbl = pm->table;

    if (!port_wait_ready(port)) return -1;

    hdr[AHCI_CMD_SLOT].prdtl = 1;
    hdr[AHCI_CMD_SLOT].prdbc = 0;

    memset(tbl, 0, sizeof(hba_cmd_table_t));

    uintptr_t buf_phys = virt_to_phys(buf);
    uint32_t byte_count = (uint32_t)(sectors * AHCI_SECTOR_SIZE);
    if (byte_count == 0 || byte_count > AHCI_MAX_PRDT_BYTES) return -1;
    tbl->prdt[0].dba = (uint32_t)(buf_phys & 0xFFFFFFFFu);
    tbl->prdt[0].dbau = (uint32_t)(buf_phys >> 32);
    tbl->prdt[0].dbc_i = (byte_count - 1u) | (1u << 31);

    fis_reg_h2d_t *cmd = (fis_reg_h2d_t *)tbl->cfis;
    memset(cmd, 0, sizeof(fis_reg_h2d_t));
    cmd->fis_type = 0x27; /* Register H2D */
    cmd->pmport_c = (1u << 7); /* Command */
    cmd->command = 0x25; /* READ DMA EXT */
    cmd->lba0 = (uint8_t)(lba & 0xFFu);
    cmd->lba1 = (uint8_t)((lba >> 8) & 0xFFu);
    cmd->lba2 = (uint8_t)((lba >> 16) & 0xFFu);
    cmd->lba3 = (uint8_t)((lba >> 24) & 0xFFu);
    cmd->lba4 = (uint8_t)((lba >> 32) & 0xFFu);
    cmd->lba5 = (uint8_t)((lba >> 40) & 0xFFu);
    cmd->device = (1u << 6);
    cmd->countl = (uint8_t)(sectors & 0xFFu);
    cmd->counth = (uint8_t)((sectors >> 8) & 0xFFu);

    port_write32(port, AHCI_P_IS, 0xFFFFFFFFu);
    port_write32(port, AHCI_P_CI, 1u << AHCI_CMD_SLOT);

    for (uint32_t i = 0; i < 200000; ++i) {
        uint32_t ci = port_read32(port, AHCI_P_CI);
        if ((ci & (1u << AHCI_CMD_SLOT)) == 0) break;
        if (port_read32(port, AHCI_P_IS) & AHCI_P_IS_TFES) break;
    }

    uint32_t is = port_read32(port, AHCI_P_IS);
    uint32_t tfd = port_read32(port, AHCI_P_TFD);
    if ((is & AHCI_P_IS_TFES) || (tfd & AHCI_TFD_ERR)) {
        port_write32(port, AHCI_P_IS, AHCI_P_IS_TFES);
        return -1;
    }
    return 0;
}

int ahci_init(void) {
    if (g_inited) return 0;

#ifdef HOST_TEST
    for (size_t s = 0; s < AHCI_FAKE_SECTORS; ++s) {
        for (size_t i = 0; i < AHCI_SECTOR_SIZE; ++i) {
            g_fake_disk[s * AHCI_SECTOR_SIZE + i] = (uint8_t)((s ^ i) & 0xFFu);
        }
    }
    g_fake_mode = true;
    g_active_port = 0;
    g_inited = true;
    klog_printf(KLOG_INFO, "ahci: host-test mode, fake disk ready");
    return 0;
#endif

    pci_config_init(0);
    uintptr_t bar_phys = 0;
    if (!find_controller(&bar_phys)) {
        klog_printf(KLOG_ERROR, "ahci: controller not found");
        return -1;
    }

    g_hba_phys = bar_phys;
    hba_write32(AHCI_REG_GHC, hba_read32(AHCI_REG_GHC) | AHCI_GHC_AE);
    g_hba_ports = hba_read32(AHCI_REG_PI);

    for (int p = 0; p < AHCI_MAX_PORTS; ++p) {
        if ((g_hba_ports & (1u << p)) == 0) continue;
        if (!port_has_device(p)) continue;
        if (configure_port(p) == 0) {
            g_active_port = p;
            break;
        }
    }

    if (g_active_port < 0) {
        klog_printf(KLOG_ERROR, "ahci: no active ports with attached devices");
        return -1;
    }

    g_inited = true;
    klog_printf(KLOG_INFO, "ahci: controller %02x:%02x.%u port %d online (PI=0x%08x)",
                g_hba_bus, g_hba_slot, g_hba_func, g_active_port, g_hba_ports);
    return 0;
}

int ahci_read_lba(uint64_t lba, void *buf, size_t sectors) {
    if (!buf || sectors == 0) return -1;

#ifdef HOST_TEST
    if (!g_inited) {
        if (ahci_init() != 0) return -1;
    }
    if (!g_fake_mode) return -1;
    if (lba + sectors > AHCI_FAKE_SECTORS) return -1;
    memcpy(buf, &g_fake_disk[lba * AHCI_SECTOR_SIZE], sectors * AHCI_SECTOR_SIZE);
    return 0;
#endif

    if (!g_inited || g_active_port < 0) return -1;
    return issue_read(g_active_port, lba, buf, sectors);
}
