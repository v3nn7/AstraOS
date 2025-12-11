#include "../include/xhci.h"
#include "../include/msi.h"
#include "../include/msi_defs.h"
#include "../include/usb_defs.h"
#include "util/io.hpp"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern uint32_t mmio_read32(uintptr_t addr);
extern void mmio_write32(uintptr_t addr, uint32_t val);
extern void klog(const char* msg);
extern void* kmemalign(size_t alignment, size_t size);
extern void* memset(void* dest, int val, size_t n);
extern uintptr_t virt_to_phys(const void* p);

/* USB Legacy Support register bits. */
#define XHCI_LEGSUP_BIOS_OWNED (1u << 16)
#define XHCI_LEGSUP_OS_OWNED   (1u << 24)

static inline uint32_t xhci_read32(uintptr_t base, uint32_t off) {
    return mmio_read32(base + off);
}

static inline void xhci_write32(uintptr_t base, uint32_t off, uint32_t val) {
    mmio_write32(base + off, val);
}

static void busy_sleep(uint32_t iterations) {
    for (uint32_t i = 0; i < iterations; ++i) {
        __asm__ __volatile__("pause");
    }
}

bool xhci_legacy_handoff(uintptr_t mmio_base) {
    if (mmio_base == 0) {
        klog("xhci: no MMIO base, skip legacy handoff");
        return true;
    }

    volatile xhci_capability_regs_t* cap = (volatile xhci_capability_regs_t*)mmio_base;
    uint32_t hccparams1 = cap->hccparams1;
    uint32_t ext_off = ((hccparams1 >> 16) & 0xFFFFu) * 4u;
    if (ext_off == 0) {
        klog("xhci: no extended caps, legacy handoff skipped");
        return true;
    }

    uint32_t legsup = xhci_read32(mmio_base, ext_off);
    legsup |= XHCI_LEGSUP_OS_OWNED;
    xhci_write32(mmio_base, ext_off, legsup);

    // Wait for BIOS to clear BIOS_OWNED.
    for (int i = 0; i < 100000; ++i) {
        uint32_t v = xhci_read32(mmio_base, ext_off);
        if ((v & XHCI_LEGSUP_BIOS_OWNED) == 0) {
            klog("xhci: legacy handoff complete");
            return true;
        }
        busy_sleep(1000);
    }
    klog("xhci: legacy handoff timeout (BIOS still owns)");
    return false;
}

bool xhci_reset_with_delay(uintptr_t mmio_base, uint8_t cap_length) {
    if (mmio_base == 0) {
        return true;
    }
    uintptr_t op_base = mmio_base + cap_length;
    uint32_t usbcmd = xhci_read32(op_base, 0x00);
    usbcmd |= (1u << 1);  // HC_RESET
    xhci_write32(op_base, 0x00, usbcmd);

    for (int i = 0; i < 5000; ++i) {  // up to ~5ms with pause
        uint32_t sts = xhci_read32(op_base, 0x04);
        if ((sts & (1u << 11)) == 0) {  // HCHalted cleared
            return true;
        }
        busy_sleep(1000);
    }
    klog("xhci: reset wait timeout");
    return false;
}

bool xhci_require_alignment(uint64_t ptr, uint64_t align) {
    return (ptr % align) == 0;
}

bool xhci_configure_msi(msi_config_t* cfg_out) {
    if (!cfg_out) {
        return false;
    }
    msi_init_config(cfg_out);
    return msi_enable(cfg_out);
}

bool xhci_check_lowmem(uint64_t phys_addr) {
    return phys_addr < 0x100000000ULL;
}

bool xhci_route_ports(uintptr_t mmio_base) {
    (void)mmio_base;
    klog("xhci: port routing requested (no-op on host)");
    return true;
}

bool xhci_controller_init(xhci_controller_t* ctrl, uintptr_t mmio_base) {
    if (!ctrl) {
        return false;
    }
    ctrl->mmio_base = mmio_base;
    ctrl->cap_regs = (xhci_capability_regs_t*)mmio_base;
    uint8_t caplen = 0;
    if (mmio_base != 0) {
        caplen = ctrl->cap_regs->caplength;
        ctrl->op_regs = (xhci_operational_regs_t*)(mmio_base + caplen);
    } else {
        caplen = 0x40;
        ctrl->op_regs = (xhci_operational_regs_t*)0;
    }

    /* Handoff and reset. */
    if (!xhci_legacy_handoff(mmio_base)) {
        return false;
    }
    if (!xhci_reset_with_delay(mmio_base, caplen)) {
        return false;
    }
    xhci_route_ports(mmio_base);
    xhci_configure_msi(&ctrl->msi_cfg);

    /* Allocate DCBAA and rings. */
    void* dcbaa = kmemalign(64, sizeof(uint64_t) * 256);
    if (!dcbaa) {
        return false;
    }
    memset(dcbaa, 0, sizeof(uint64_t) * 256);
    if (ctrl->op_regs) {
        ctrl->op_regs->dcbaap = virt_to_phys(dcbaa);
    }

    if (!xhci_ring_init(&ctrl->cmd_ring, 64)) {
        return false;
    }
    if (!xhci_event_ring_init(&ctrl->evt_ring, 64)) {
        return false;
    }

    /* Program CRCR and ERST registers if accessible. */
    uint64_t cmd_phys = virt_to_phys(ctrl->cmd_ring.trbs);
    if (ctrl->op_regs) {
        ctrl->op_regs->crcr = cmd_phys | 1u;  // set RCS
    }

    if (mmio_base != 0) {
        uintptr_t rtsoff = ((ctrl->cap_regs->rtsoff >> 5) << 5) + mmio_base;
        xhci_runtime_regs_t* rt = (xhci_runtime_regs_t*)rtsoff;
        rt->intr[0].erstsz = ctrl->evt_ring.erst_size;
        rt->intr[0].erstba = virt_to_phys(ctrl->evt_ring.erst);
        rt->intr[0].erdp = virt_to_phys(ctrl->evt_ring.ring.trbs);
        rt->intr[0].iman |= 1u;  // enable interrupts
        ctrl->op_regs->usbcmd |= XHCI_USBCMD_RUN;
    }
    return true;
}
