/**
 * XHCI Complete Initialization Sequence
 * 
 * Production-grade XHCI 1.2 initialization implementation following
 * the EXACT requirements of XHCI 1.2 Specification, Section 4:
 * Host Controller Initialization.
 * 
 * References:
 * - XHCI 1.2 Specification, Section 4.2: Initialization
 * - XHCI 1.2 Specification, Section 5.3: Command Ring
 * - XHCI 1.2 Specification, Section 5.4: Event Ring
 * 
 * CRITICAL: This implementation follows the EXACT register write ordering
 * required by the specification to ensure CSS synchronization succeeds.
 * 
 * Why CSS synchronization was timing out:
 * 1. CRCR was written before DCBAAP and CONFIG were valid
 * 2. CRCR was not cleared before writing new pointer
 * 3. Controller was not properly halted before CRCR write
 * 4. Memory barriers were missing between critical register writes
 * 5. Event ring was not fully configured before CRCR write
 */

#include "usb/xhci.h"
#include "usb/xhci_internal.h"
#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "mmio.h"
#include "vmm.h"
#include "memory.h"
#include "pmm.h"

/* offsetof macro for bare-metal kernel */
#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

/* MMIO Access Helpers */
#define XHCI_READ32(r, off)     mmio_read32((volatile uint32_t *)((uintptr_t)(r) + (off)))
#define XHCI_WRITE32(r, off, v) mmio_write32((volatile uint32_t *)((uintptr_t)(r) + (off)), v)
#define XHCI_READ64(r, off)     mmio_read64((volatile uint64_t *)((uintptr_t)(r) + (off)))
#define XHCI_WRITE64(r, off, v) mmio_write64((volatile uint64_t *)((uintptr_t)(r) + (off)), v)

/* External functions */
extern void xhci_flush_cache(void *addr, size_t sz);
extern void xhci_build_link_trb(xhci_trb_t *trb, uint64_t next_ring_addr, bool toggle_cycle);

/* Forward declarations */
static int xhci_wait_for_bit(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t timeout_us, const char *name);
static int xhci_wait_for_bit64(volatile uint64_t *reg, uint64_t mask, uint64_t expected, uint32_t timeout_us, const char *name);
static int xhci_parse_capabilities(xhci_controller_t *xhci);
static int xhci_reset_controller_complete(xhci_controller_t *xhci);
static int xhci_setup_memory_structures(xhci_controller_t *xhci);
static int xhci_setup_command_ring_complete(xhci_controller_t *xhci);
static int xhci_setup_event_ring_complete(xhci_controller_t *xhci);

/**
 * Wait for a bit pattern in a 32-bit register
 * 
 * @param reg Register to poll
 * @param mask Bit mask to check
 * @param expected Expected value (after masking)
 * @param timeout_us Timeout in microseconds
 * @param name Name for logging
 * @return 0 on success, -1 on timeout
 */
static int xhci_wait_for_bit(volatile uint32_t *reg, uint32_t mask, uint32_t expected, uint32_t timeout_us, const char *name) {
    uint32_t loops = 0;
    uint32_t max_loops = timeout_us / 100; /* Approximate microsecond timing */
    
    while (loops < max_loops) {
        uint32_t value = *reg;
        if ((value & mask) == expected) {
            if (loops > 0) {
                klog_printf(KLOG_DEBUG, "xhci: %s: waited %u loops", name, loops);
            }
            return 0;
        }
        for (volatile int i = 0; i < 100; i++);
        loops++;
    }
    
    uint32_t final_value = *reg;
    klog_printf(KLOG_ERROR, "xhci: %s timeout! reg=0x%08x mask=0x%08x expected=0x%08x got=0x%08x",
                name, (uint32_t)(uintptr_t)reg, mask, expected, final_value);
    return -1;
}

/**
 * Wait for a bit pattern in a 64-bit register
 */
static int xhci_wait_for_bit64(volatile uint64_t *reg, uint64_t mask, uint64_t expected, uint32_t timeout_us, const char *name) {
    uint32_t loops = 0;
    uint32_t max_loops = timeout_us / 100;
    
    while (loops < max_loops) {
        uint64_t value = *reg;
        if ((value & mask) == expected) {
            if (loops > 0) {
                klog_printf(KLOG_DEBUG, "xhci: %s: waited %u loops", name, loops);
            }
            return 0;
        }
        for (volatile int i = 0; i < 100; i++);
        loops++;
    }
    
    uint64_t final_value = *reg;
    klog_printf(KLOG_ERROR, "xhci: %s timeout! reg=0x%016llx mask=0x%016llx expected=0x%016llx got=0x%016llx",
                name, (unsigned long long)(uintptr_t)reg, (unsigned long long)mask,
                (unsigned long long)expected, (unsigned long long)final_value);
    return -1;
}

/**
 * Parse XHCI capability registers
 * 
 * XHCI 1.2 Spec Section 5.2: Capability Registers
 */
static int xhci_parse_capabilities(xhci_controller_t *xhci) {
    if (!xhci || !xhci->cap_regs) {
        klog_printf(KLOG_ERROR, "xhci: invalid controller for capability parsing");
        return -1;
    }
    
    xhci_cap_regs_t *cap = xhci->cap_regs;
    
    /* Read CAPLENGTH (offset 0x00) - XHCI 1.2 Spec Section 5.2.1 */
    uint32_t caplen = cap->CAPLENGTH & 0xFF;
    if (caplen < 0x20) {
        klog_printf(KLOG_ERROR, "xhci: invalid CAPLENGTH=%u (minimum 0x20)", caplen);
        return -1;
    }
    xhci->cap_length = caplen;
    
    /* Compute operational registers base - XHCI 1.2 Spec Section 5.2.2 */
    xhci->op_regs = (xhci_op_regs_t *)((uintptr_t)cap + caplen);
    
    /* Read HCIVERSION (offset 0x02) - XHCI 1.2 Spec Section 5.2.3 */
    xhci->hci_version = cap->HCIVERSION;
    
    /* Read HCSPARAMS1 (offset 0x04) - XHCI 1.2 Spec Section 5.2.4 */
    uint32_t p1 = cap->HCSPARAMS1;
    xhci->num_slots = p1 & 0xFF;
    xhci->num_ports = (p1 >> 24) & 0xFF;
    xhci->max_interrupters = (p1 >> 8) & 0x7FF;
    
    /* Read HCCPARAMS1 (offset 0x10) - XHCI 1.2 Spec Section 5.2.5 */
    uint32_t hccp1 = cap->HCCPARAMS1;
    xhci->has_64bit_addressing = (hccp1 & 1) ? 1 : 0;
    xhci->has_scratchpad = (hccp1 & (1 << 2)) ? 1 : 0;
    
    /* Read HCSPARAMS2 for scratchpad size if needed */
    if (xhci->has_scratchpad) {
        uint32_t p2 = cap->HCSPARAMS2;
        xhci->scratchpad_size = ((p2 >> 21) & 0x1F) << 5 | ((p2 >> 16) & 0x1F);
        klog_printf(KLOG_INFO, "xhci: scratchpad enabled (%u buffers)", xhci->scratchpad_size);
    }
    
    /* Compute runtime registers base - XHCI 1.2 Spec Section 5.2.6 */
    uint32_t rtsoff = cap->RTSOFF & 0xFFFFFFE0; /* Align to 32 bytes */
    xhci->rt_regs = (xhci_rt_regs_t *)((uintptr_t)cap + rtsoff);
    xhci->intr0 = &xhci->rt_regs->ir[0];
    
    /* Compute doorbell registers base - XHCI 1.2 Spec Section 5.2.7 */
    uint32_t dboff = cap->DBOFF & ~0x3; /* Remove lowest 2 bits */
    xhci->doorbell_regs = (xhci_doorbell_regs_t *)((uintptr_t)cap + dboff);
    
    /* Validate register offsets */
    klog_printf(KLOG_INFO, "xhci: capability parsing:");
    klog_printf(KLOG_INFO, "  CAPLENGTH=%u HCIVERSION=0x%04x", caplen, xhci->hci_version);
    klog_printf(KLOG_INFO, "  slots=%u ports=%u interrupters=%u", 
                xhci->num_slots, xhci->num_ports, xhci->max_interrupters);
    klog_printf(KLOG_INFO, "  64-bit=%d scratchpad=%d", 
                xhci->has_64bit_addressing, xhci->has_scratchpad);
    klog_printf(KLOG_INFO, "  op_regs offset=0x%x rt_regs offset=0x%x doorbell offset=0x%x",
                caplen, rtsoff, dboff);
    
    /* Validate CRCR offset - MUST be 0x18 per XHCI 1.2 Spec Section 5.3.1 */
    if (offsetof(xhci_op_regs_t, CRCR) != 0x18) {
        klog_printf(KLOG_ERROR, "xhci: CRCR offset mismatch! expected=0x18 got=0x%zx",
                    offsetof(xhci_op_regs_t, CRCR));
        return -1;
    }
    
    return 0;
}

/**
 * Complete controller reset sequence
 * 
 * XHCI 1.2 Spec Section 4.2.1: Controller Reset
 * 
 * Steps:
 * 1. Stop controller (USBCMD.RS=0, wait for USBSTS.HCH=1)
 * 2. Issue HC reset (USBCMD.HCRST=1, wait for it to clear)
 * 3. Wait for controller ready (USBSTS.CNR=0)
 */
static int xhci_reset_controller_complete(xhci_controller_t *xhci) {
    if (!xhci || !xhci->op_regs) {
        klog_printf(KLOG_ERROR, "xhci: invalid controller for reset");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: performing complete controller reset...");
    
    /* Step 1: Stop controller - XHCI 1.2 Spec Section 4.2.1.1 */
    uint32_t usbcmd = xhci->op_regs->USBCMD;
    uint32_t usbsts = xhci->op_regs->USBSTS;
    
    klog_printf(KLOG_DEBUG, "xhci: reset step 1: USBCMD=0x%08x USBSTS=0x%08x (before stop)",
                usbcmd, usbsts);
    
    /* Clear RUNSTOP bit (bit 0) */
    usbcmd &= ~XHCI_CMD_RUN;
    xhci->op_regs->USBCMD = usbcmd;
    __asm__ volatile("mfence" ::: "memory");
    
    /* Wait for HCH (Host Controller Halted) - XHCI 1.2 Spec Section 4.2.1.2 */
    if (xhci_wait_for_bit(&xhci->op_regs->USBSTS, XHCI_STS_HCH, XHCI_STS_HCH, 200000, "HCH") < 0) {
        klog_printf(KLOG_ERROR, "xhci: controller halt timeout");
        return -1;
    }
    
    usbsts = xhci->op_regs->USBSTS;
    klog_printf(KLOG_DEBUG, "xhci: reset step 1 complete: controller halted USBSTS=0x%08x", usbsts);
    
    /* Step 2: Issue HC reset - XHCI 1.2 Spec Section 4.2.1.3 */
    usbcmd = xhci->op_regs->USBCMD;
    usbcmd |= XHCI_CMD_HCRST;
    xhci->op_regs->USBCMD = usbcmd;
    __asm__ volatile("mfence" ::: "memory");
    
    klog_printf(KLOG_DEBUG, "xhci: reset step 2: HCRST set, waiting for clear...");
    
    /* Wait for HCRST to clear - XHCI 1.2 Spec Section 4.2.1.4 */
    if (xhci_wait_for_bit(&xhci->op_regs->USBCMD, XHCI_CMD_HCRST, 0, 200000, "HCRST clear") < 0) {
        klog_printf(KLOG_ERROR, "xhci: HC reset timeout");
        return -1;
    }
    
    klog_printf(KLOG_DEBUG, "xhci: reset step 2 complete: HCRST cleared");
    
    /* Step 3: Wait for controller ready - XHCI 1.2 Spec Section 4.2.1.5 */
    if (xhci_wait_for_bit(&xhci->op_regs->USBSTS, XHCI_STS_CNR, 0, 200000, "CNR clear") < 0) {
        klog_printf(KLOG_ERROR, "xhci: controller not ready timeout");
        return -1;
    }
    
    usbsts = xhci->op_regs->USBSTS;
    klog_printf(KLOG_INFO, "xhci: reset complete: USBSTS=0x%08x (HCH=%u CNR=%u)",
                usbsts, (usbsts & XHCI_STS_HCH) ? 1 : 0, (usbsts & XHCI_STS_CNR) ? 1 : 0);
    
    return 0;
}

/**
 * Setup memory structures (DCBAA, Scratchpad)
 * 
 * XHCI 1.2 Spec Section 4.2.2: Memory Setup
 */
static int xhci_setup_memory_structures(xhci_controller_t *xhci) {
    if (!xhci) {
        klog_printf(KLOG_ERROR, "xhci: invalid controller for memory setup");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: setting up memory structures...");
    
    /* Allocate DCBAA (Device Context Base Address Array) - XHCI 1.2 Spec Section 4.2.2.1
     * Size: (num_slots + 1) * 8 bytes, must be 64-byte aligned
     */
    size_t dc_sz = (xhci->num_slots + 1) * sizeof(uint64_t);
    void *dc_alloc = kmalloc(dc_sz + 64);
    if (!dc_alloc) {
        klog_printf(KLOG_ERROR, "xhci: DCBAA allocation failed");
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t dc_addr = (uintptr_t)dc_alloc;
    if (dc_addr % 64 != 0) {
        dc_addr = (dc_addr + 63) & ~63UL;
    }
    xhci->dcbaap = (uint64_t *)dc_addr;
    k_memset(xhci->dcbaap, 0, dc_sz);
    xhci_flush_cache(xhci->dcbaap, dc_sz);
    
    klog_printf(KLOG_INFO, "xhci: DCBAA allocated: %zu bytes at virt=%p (64-byte aligned)",
                dc_sz, xhci->dcbaap);
    
    /* Setup scratchpad buffers if needed - XHCI 1.2 Spec Section 4.2.2.2 */
    if (xhci->has_scratchpad && xhci->scratchpad_size > 0) {
        /* Allocate pointer array for scratchpad buffers */
        xhci->scratchpad_buffers = (void **)kmalloc(xhci->scratchpad_size * sizeof(void *));
        if (!xhci->scratchpad_buffers) {
            klog_printf(KLOG_ERROR, "xhci: scratchpad buffer pointer array allocation failed");
            kfree(dc_alloc);
            return -1;
        }
        
        /* Allocate scratchpad array page */
        void *scratch_arr = kmalloc(PAGE_SIZE);
        if (!scratch_arr) {
            klog_printf(KLOG_ERROR, "xhci: scratchpad array page allocation failed");
            kfree(xhci->scratchpad_buffers);
            kfree(dc_alloc);
            return -1;
        }
        k_memset(scratch_arr, 0, PAGE_SIZE);
        uint64_t *sptr = (uint64_t *)scratch_arr;
        
        /* Allocate individual scratchpad pages */
        for (uint32_t i = 0; i < xhci->scratchpad_size; i++) {
            void *p = kmalloc(PAGE_SIZE);
            if (!p) {
                klog_printf(KLOG_ERROR, "xhci: scratchpad page %u allocation failed", i);
                /* Cleanup */
                for (uint32_t j = 0; j < i; j++) {
                    kfree(xhci->scratchpad_buffers[j]);
                }
                kfree(xhci->scratchpad_buffers);
                kfree(scratch_arr);
                kfree(dc_alloc);
                return -1;
            }
            k_memset(p, 0, PAGE_SIZE);
            xhci_flush_cache(p, PAGE_SIZE);
            xhci->scratchpad_buffers[i] = p;
            
            /* Get physical address */
            uint64_t virt = (uint64_t)(uintptr_t)p;
            uint64_t phys = vmm_virt_to_phys(virt);
            if (phys == 0) {
                extern uint64_t pmm_hhdm_offset;
                if (pmm_hhdm_offset && virt >= pmm_hhdm_offset) {
                    phys = virt - pmm_hhdm_offset;
                } else {
                    klog_printf(KLOG_ERROR, "xhci: failed to get physical address for scratchpad page %u", i);
                    /* Cleanup */
                    for (uint32_t j = 0; j <= i; j++) {
                        kfree(xhci->scratchpad_buffers[j]);
                    }
                    kfree(xhci->scratchpad_buffers);
                    kfree(scratch_arr);
                    kfree(dc_alloc);
                    return -1;
                }
            }
            sptr[i] = phys;
        }
        
        xhci_flush_cache(sptr, PAGE_SIZE);
        
        /* Get physical address of scratchpad array */
        uint64_t scratch_virt = (uint64_t)(uintptr_t)scratch_arr;
        uint64_t scratch_phys = vmm_virt_to_phys(scratch_virt);
        if (scratch_phys == 0) {
            extern uint64_t pmm_hhdm_offset;
            if (pmm_hhdm_offset && scratch_virt >= pmm_hhdm_offset) {
                scratch_phys = scratch_virt - pmm_hhdm_offset;
            } else {
                klog_printf(KLOG_ERROR, "xhci: failed to get physical address for scratchpad array");
                /* Cleanup */
                for (uint32_t i = 0; i < xhci->scratchpad_size; i++) {
                    kfree(xhci->scratchpad_buffers[i]);
                }
                kfree(xhci->scratchpad_buffers);
                kfree(scratch_arr);
                kfree(dc_alloc);
                return -1;
            }
        }
        
        /* DCBAAP[0] points to scratchpad array - XHCI 1.2 Spec Section 4.2.2.3 */
        xhci->dcbaap[0] = scratch_phys;
        xhci_flush_cache(&xhci->dcbaap[0], sizeof(uint64_t));
        
        klog_printf(KLOG_INFO, "xhci: scratchpad setup complete: %u pages array_phys=0x%016llx",
                    xhci->scratchpad_size, (unsigned long long)scratch_phys);
    }
    
    return 0;
}

/**
 * Setup command ring with proper Link TRB
 * 
 * XHCI 1.2 Spec Section 5.3: Command Ring
 */
static int xhci_setup_command_ring_complete(xhci_controller_t *xhci) {
    if (!xhci) {
        klog_printf(KLOG_ERROR, "xhci: invalid controller for command ring setup");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: setting up command ring...");
    
    /* Allocate command ring buffer - must be 64-byte aligned */
    uint32_t ring_size = 256; /* Power of 2 */
    size_t trb_array_size = ring_size * 16; /* 16 bytes per TRB */
    void *ring_alloc = kmalloc(trb_array_size + 64);
    if (!ring_alloc) {
        klog_printf(KLOG_ERROR, "xhci: command ring allocation failed");
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t ring_addr = (uintptr_t)ring_alloc;
    if (ring_addr % 64 != 0) {
        ring_addr = (ring_addr + 63) & ~63;
    }
    xhci->cmd_ring.trbs = (xhci_trb_t *)ring_addr;
    k_memset(xhci->cmd_ring.trbs, 0, trb_array_size);
    
    xhci->cmd_ring.size = ring_size;
    xhci->cmd_ring.dequeue = 0;
    xhci->cmd_ring.enqueue = 0;
    xhci->cmd_ring.cycle_state = true; /* Start with cycle = 1 */
    
    /* Get physical address */
    uint64_t virt_addr = (uint64_t)(uintptr_t)xhci->cmd_ring.trbs;
    uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
    if (phys_addr == 0) {
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
            phys_addr = virt_addr - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci: failed to get physical address for command ring");
            kfree(ring_alloc);
            return -1;
        }
    }
    
    /* Verify 64-byte alignment */
    if ((phys_addr & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci: command ring not 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)phys_addr);
        kfree(ring_alloc);
        return -1;
    }
    
    xhci->cmd_ring.phys_addr = (phys_addr_t)phys_addr;
    
    /* Add Link TRB at the end - XHCI 1.2 Spec Section 5.3.2 */
    uint32_t last_index = ring_size - 1;
    xhci_trb_t *link_trb = &xhci->cmd_ring.trbs[last_index];
    
    /* Build Link TRB with TC=1 - XHCI 1.2 Spec Section 5.3.2.1
     * TC (Toggle Cycle) bit must be set to toggle CSS when HC processes Link TRB
     */
    xhci_build_link_trb(link_trb, phys_addr, true); /* toggle_cycle = 1 */
    
    /* CRITICAL: Link TRB cycle bit must match ring cycle_state (which is 1) */
    link_trb->control |= XHCI_TRB_CYCLE;
    
    /* Flush cache for command ring - HC must see Link TRB */
    xhci_flush_cache(xhci->cmd_ring.trbs, trb_array_size);
    
    klog_printf(KLOG_INFO, "xhci: command ring setup complete:");
    klog_printf(KLOG_INFO, "  size=%u phys=0x%016llx (aligned=%d)",
                ring_size, (unsigned long long)phys_addr, ((phys_addr & 0x3F) == 0) ? 1 : 0);
    klog_printf(KLOG_INFO, "  link_trb[%u]: phys=0x%016llx TC=%u cycle=%u",
                last_index, (unsigned long long)phys_addr,
                (link_trb->control & XHCI_TRB_TC) ? 1 : 0,
                (link_trb->control & XHCI_TRB_CYCLE) ? 1 : 0);
    
    return 0;
}

/**
 * Setup event ring and ERST table
 * 
 * XHCI 1.2 Spec Section 5.4: Event Ring
 */
static int xhci_setup_event_ring_complete(xhci_controller_t *xhci) {
    if (!xhci) {
        klog_printf(KLOG_ERROR, "xhci: invalid controller for event ring setup");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: setting up event ring...");
    
    /* Allocate event ring buffer - must be 64-byte aligned */
    uint32_t ring_size = 256; /* Power of 2 */
    size_t trb_array_size = ring_size * sizeof(xhci_event_trb_t);
    void *ring_alloc = kmalloc(trb_array_size + 64);
    if (!ring_alloc) {
        klog_printf(KLOG_ERROR, "xhci: event ring allocation failed");
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t ring_addr = (uintptr_t)ring_alloc;
    if (ring_addr % 64 != 0) {
        ring_addr = (ring_addr + 63) & ~63;
    }
    xhci->event_ring.trbs = (xhci_event_trb_t *)ring_addr;
    k_memset(xhci->event_ring.trbs, 0, trb_array_size);
    
    xhci->event_ring.size = ring_size;
    xhci->event_ring.dequeue = 0;
    xhci->event_ring.enqueue = 0;
    xhci->event_ring.cycle_state = true; /* Start with cycle = 1 */
    
    /* Get physical address */
    uint64_t virt_addr = (uint64_t)(uintptr_t)xhci->event_ring.trbs;
    uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
    if (phys_addr == 0) {
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
            phys_addr = virt_addr - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci: failed to get physical address for event ring");
            kfree(ring_alloc);
            return -1;
        }
    }
    
    /* Verify 64-byte alignment */
    if ((phys_addr & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci: event ring not 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)phys_addr);
        kfree(ring_alloc);
        return -1;
    }
    
    xhci->event_ring.phys_addr = (phys_addr_t)phys_addr;
    
    /* Allocate ERST (Event Ring Segment Table) - XHCI 1.2 Spec Section 5.4.2
     * ERST must be 64-byte aligned
     */
    size_t erst_size = sizeof(xhci_erst_entry_t) * 1; /* Single segment */
    void *erst_alloc = kmalloc(erst_size + 64);
    if (!erst_alloc) {
        klog_printf(KLOG_ERROR, "xhci: ERST allocation failed");
        kfree(ring_alloc);
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t erst_addr = (uintptr_t)erst_alloc;
    if (erst_addr % 64 != 0) {
        erst_addr = (erst_addr + 63) & ~63;
    }
    xhci_erst_entry_t *erst = (xhci_erst_entry_t *)erst_addr;
    k_memset(erst, 0, erst_size);
    
    /* Setup ERST entry - XHCI 1.2 Spec Section 5.4.2.1 */
    erst[0].ring_segment_base = phys_addr;
    erst[0].ring_segment_size = ring_size; /* Number of TRBs */
    erst[0].reserved = 0;
    erst[0].reserved2 = 0;
    
    xhci->event_ring.segment_table = erst;
    
    /* Get physical address of ERST */
    uint64_t erst_virt = (uint64_t)(uintptr_t)erst;
    uint64_t erst_phys = vmm_virt_to_phys(erst_virt);
    if (erst_phys == 0) {
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && erst_virt >= pmm_hhdm_offset) {
            erst_phys = erst_virt - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci: failed to get physical address for ERST");
            kfree(erst_alloc);
            kfree(ring_alloc);
            return -1;
        }
    }
    
    /* Verify ERST is 64-byte aligned */
    if ((erst_phys & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci: ERST not 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)erst_phys);
        kfree(erst_alloc);
        kfree(ring_alloc);
        return -1;
    }
    
    xhci->event_ring.segment_table_phys = (phys_addr_t)erst_phys;
    
    /* Flush cache for ERST - HC must see ERST table */
    xhci_flush_cache(erst, erst_size);
    
    klog_printf(KLOG_INFO, "xhci: event ring setup complete:");
    klog_printf(KLOG_INFO, "  size=%u phys=0x%016llx (aligned=%d)",
                ring_size, (unsigned long long)phys_addr, ((phys_addr & 0x3F) == 0) ? 1 : 0);
    klog_printf(KLOG_INFO, "  ERST: phys=0x%016llx (aligned=%d) entry[0].base=0x%016llx size=%u",
                (unsigned long long)erst_phys, ((erst_phys & 0x3F) == 0) ? 1 : 0,
                (unsigned long long)erst[0].ring_segment_base, erst[0].ring_segment_size);
    
    return 0;
}

/**
 * Complete XHCI initialization following EXACT XHCI 1.2 spec ordering
 * 
 * XHCI 1.2 Spec Section 4.2: Initialization Sequence
 * 
 * CRITICAL ORDERING (per spec Section 4.2.3):
 * 
 * Phase 1: Preparation (controller must be halted)
 * 1. Parse capability registers
 * 2. Reset controller (HALT → HCRST → wait for CNR=0)
 * 
 * Phase 2: Memory Allocation
 * 3. Allocate DCBAA
 * 4. Allocate Scratchpad (if needed)
 * 5. Allocate Command Ring (with Link TRB, TC=1)
 * 6. Allocate Event Ring + ERST
 * 
 * Phase 3: Register Configuration (EXACT ORDER REQUIRED)
 * 7. Write DCBAAP (must be first)
 * 8. Write CONFIG (must be second, before CRCR)
 * 9. Write ERSTSZ
 * 10. Write ERSTBA
 * 11. Write ERDP (clear EHB bit)
 * 12. Write CRCR (ONLY after DCBAAP and CONFIG are valid)
 * 
 * Phase 4: Interrupts and Start
 * 13. Enable interrupts (IMAN.IE=1, IMOD)
 * 14. Start controller (USBCMD.RS=1, USBCMD.INTE=1)
 * 15. Wait for HCH=0
 * 
 * Why CSS synchronization was timing out:
 * - CRCR was written before DCBAAP and CONFIG were valid
 * - CRCR was not cleared before writing new pointer
 * - Controller was not properly halted before CRCR write
 * - Memory barriers were missing between register writes
 * - Event ring configuration was incomplete
 */
int xhci_init_complete(usb_host_controller_t *hc) {
    if (!hc || !hc->regs_base) {
        klog_printf(KLOG_ERROR, "xhci: invalid host controller");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: ========================================");
    klog_printf(KLOG_INFO, "xhci: Complete Initialization Sequence Start");
    klog_printf(KLOG_INFO, "xhci: ========================================");
    
    /* Allocate controller structure */
    xhci_controller_t *xhci = (xhci_controller_t *)hc->private_data;
    if (!xhci) {
        xhci = (xhci_controller_t *)kmalloc(sizeof(xhci_controller_t));
        if (!xhci) {
            klog_printf(KLOG_ERROR, "xhci: controller allocation failed");
            return -1;
        }
        k_memset(xhci, 0, sizeof(xhci_controller_t));
        hc->private_data = xhci;
    }
    
    /* ============================================================
     * Phase 1: Preparation
     * ============================================================ */
    
    /* Step 1: Parse capability registers */
    void *mmio_base = hc->regs_base;
    xhci_cap_regs_t *cap = (xhci_cap_regs_t *)mmio_base;
    xhci->cap_regs = cap;
    
    if (xhci_parse_capabilities(xhci) < 0) {
        klog_printf(KLOG_ERROR, "xhci: capability parsing failed");
        return -1;
    }
    
    /* Step 2: Reset controller - XHCI 1.2 Spec Section 4.2.1 */
    if (xhci_reset_controller_complete(xhci) < 0) {
        klog_printf(KLOG_ERROR, "xhci: controller reset failed");
        return -1;
    }
    
    /* Verify controller is halted before proceeding */
    uint32_t usbsts_check = xhci->op_regs->USBSTS;
    if (!(usbsts_check & XHCI_STS_HCH)) {
        klog_printf(KLOG_ERROR, "xhci: controller not halted! USBSTS=0x%08x", usbsts_check);
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: Phase 1 complete - controller halted and ready");
    
    /* ============================================================
     * Phase 2: Memory Allocation
     * ============================================================ */
    
    /* Step 3: Setup memory structures (DCBAA, Scratchpad) */
    if (xhci_setup_memory_structures(xhci) < 0) {
        klog_printf(KLOG_ERROR, "xhci: memory setup failed");
        return -1;
    }
    
    /* Step 4: Setup command ring */
    if (xhci_setup_command_ring_complete(xhci) < 0) {
        klog_printf(KLOG_ERROR, "xhci: command ring setup failed");
        return -1;
    }
    
    /* Step 5: Setup event ring */
    if (xhci_setup_event_ring_complete(xhci) < 0) {
        klog_printf(KLOG_ERROR, "xhci: event ring setup failed");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci: Phase 2 complete - all memory structures allocated");
    
    /* ============================================================
     * Phase 3: Register Configuration (EXACT ORDER REQUIRED)
     * XHCI 1.2 Spec Section 4.2.3
     * ============================================================ */
    
    klog_printf(KLOG_INFO, "xhci: Phase 3: Writing registers in spec-required order...");
    
    /* Step 6: Write DCBAAP - XHCI 1.2 Spec Section 5.3.5
     * MUST be written FIRST before any other operational registers
     */
    uint64_t dcb_virt = (uint64_t)(uintptr_t)xhci->dcbaap;
    uint64_t dcb_phys = vmm_virt_to_phys(dcb_virt);
    if (dcb_phys == 0) {
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && dcb_virt >= pmm_hhdm_offset) {
            dcb_phys = dcb_virt - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci: failed to get physical address for DCBAA");
            return -1;
        }
    }
    
    if ((dcb_phys & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci: DCBAA not 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)dcb_phys);
        return -1;
    }
    
    xhci->op_regs->DCBAAP = dcb_phys;
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    klog_printf(KLOG_INFO, "xhci: [1/6] DCBAAP written: 0x%016llx", (unsigned long long)dcb_phys);
    
    /* Step 7: Write CONFIG - XHCI 1.2 Spec Section 5.3.6
     * MUST be written SECOND, before CRCR
     */
    xhci->op_regs->CONFIG = xhci->num_slots;
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    klog_printf(KLOG_INFO, "xhci: [2/6] CONFIG written: %u", xhci->num_slots);
    
    /* Step 8: Write ERSTSZ - XHCI 1.2 Spec Section 5.4.3 */
    XHCI_WRITE32(xhci->rt_regs, XHCI_ERSTSZ(0), 1); /* 1 segment */
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    klog_printf(KLOG_INFO, "xhci: [3/6] ERSTSZ written: 1");
    
    /* Step 9: Write ERSTBA - XHCI 1.2 Spec Section 5.4.4 */
    XHCI_WRITE64(xhci->rt_regs, XHCI_ERSTBA(0), xhci->event_ring.segment_table_phys);
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    klog_printf(KLOG_INFO, "xhci: [4/6] ERSTBA written: 0x%016llx",
                (unsigned long long)xhci->event_ring.segment_table_phys);
    
    /* Step 10: Write ERDP (clear EHB) - XHCI 1.2 Spec Section 5.4.5
     * ERDP[3] = EHB (Event Handler Busy) must be 0 during initialization
     */
    uint64_t erdp_value = xhci->event_ring.phys_addr & ~XHCI_ERDP_EHB;
    XHCI_WRITE64(xhci->rt_regs, XHCI_ERDP(0), erdp_value);
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    klog_printf(KLOG_INFO, "xhci: [5/6] ERDP written: 0x%016llx (EHB=0)",
                (unsigned long long)erdp_value);
    
    /* Step 11: Write CRCR - XHCI 1.2 Spec Section 5.3.4
     * CRITICAL: This MUST be written LAST, only after DCBAAP and CONFIG are valid
     * Controller MUST be halted (HCH=1) before writing CRCR
     */
    usbsts_check = xhci->op_regs->USBSTS;
    if (!(usbsts_check & XHCI_STS_HCH)) {
        klog_printf(KLOG_ERROR, "xhci: controller not halted! Cannot write CRCR. USBSTS=0x%08x",
                    usbsts_check);
        return -1;
    }
    
    /* CRITICAL: Clear CRCR first - XHCI 1.2 Spec Section 5.3.4.2
     * Clear CRR, CSS, CA, and RCS bits before writing new pointer
     */
    uint64_t crcr_before = xhci->op_regs->CRCR;
    klog_printf(KLOG_DEBUG, "xhci: CRCR before clear: 0x%016llx (RCS=%u CSS=%u CRR=%u)",
                (unsigned long long)crcr_before,
                (crcr_before & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr_before & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr_before & XHCI_CRCR_CRR) ? 1 : 0);
    
    /* Clear all control bits, preserve address bits [63:6] */
    uint64_t crcr_cleared = (crcr_before & ~0x3F) & ~(XHCI_CRCR_CRR | XHCI_CRCR_CSS | XHCI_CRCR_CA | XHCI_CRCR_RCS);
    xhci->op_regs->CRCR = crcr_cleared;
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    
    /* Small delay to ensure clear is processed */
    for (volatile int i = 0; i < 1000; i++);
    
    /* Write new CRCR with RCS=1 - XHCI 1.2 Spec Section 5.3.4.3
     * CRCR[63:6] = Command Ring Pointer (physical address, 64-byte aligned)
     * CRCR[0] = RCS (Ring Cycle State) = 1
     * DO NOT set CSS - HC will set it automatically
     */
    uint64_t cmd_ring_addr = xhci->cmd_ring.phys_addr;
    uint64_t crcr_value = (cmd_ring_addr & ~0x3F) | XHCI_CRCR_RCS;
    xhci->op_regs->CRCR = crcr_value;
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    
    klog_printf(KLOG_INFO, "xhci: [6/6] CRCR written: 0x%016llx (RCS=1)",
                (unsigned long long)crcr_value);
    
    /* CRITICAL: Validate CSS synchronization - XHCI 1.2 Spec Section 5.3.4.1
     * CSS (Command Ring Cycle State) must become 1 to match RCS=1
     * If CSS does not synchronize, command ring will never process commands
     */
    klog_printf(KLOG_INFO, "xhci: waiting for CSS synchronization...");
    if (xhci_wait_for_bit64(&xhci->op_regs->CRCR, XHCI_CRCR_CSS, XHCI_CRCR_CSS, 100000, "CRCR CSS") < 0) {
        uint64_t crcr_final = xhci->op_regs->CRCR;
        klog_printf(KLOG_ERROR, "xhci: ========================================");
        klog_printf(KLOG_ERROR, "xhci: CRCR CSS SYNCHRONIZATION FAILED!");
        klog_printf(KLOG_ERROR, "xhci: ========================================");
        klog_printf(KLOG_ERROR, "xhci: CRCR=0x%016llx", (unsigned long long)crcr_final);
        klog_printf(KLOG_ERROR, "xhci: RCS=%u CSS=%u CRR=%u CA=%u",
                    (crcr_final & XHCI_CRCR_RCS) ? 1 : 0,
                    (crcr_final & XHCI_CRCR_CSS) ? 1 : 0,
                    (crcr_final & XHCI_CRCR_CRR) ? 1 : 0,
                    (crcr_final & XHCI_CRCR_CA) ? 1 : 0);
        klog_printf(KLOG_ERROR, "xhci: Command ring will NOT work!");
        return -1;
    }
    
    uint64_t crcr_final = xhci->op_regs->CRCR;
    klog_printf(KLOG_INFO, "xhci: CSS synchronized successfully!");
    klog_printf(KLOG_INFO, "xhci: CRCR final: 0x%016llx RCS=%u CSS=%u CRR=%u",
                (unsigned long long)crcr_final,
                (crcr_final & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr_final & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr_final & XHCI_CRCR_CRR) ? 1 : 0);
    
    klog_printf(KLOG_INFO, "xhci: Phase 3 complete - all registers configured");
    
    /* ============================================================
     * Phase 4: Interrupts and Start
     * ============================================================ */
    
    /* Step 12: Enable interrupts - XHCI 1.2 Spec Section 4.2.4 */
    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    iman |= (1 << 1); /* IE = 1 (Interrupt Enable) */
    iman &= ~(1 << 0); /* Clear IP (Interrupt Pending) */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman);
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    
    /* Set interrupt moderation - XHCI 1.2 Spec Section 5.4.6 */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMOD(0), 4000);
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    
    klog_printf(KLOG_INFO, "xhci: interrupts enabled: IMAN=0x%08x IMOD=4000",
                XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0)));
    
    /* Step 13: Start controller - XHCI 1.2 Spec Section 4.2.5
     * Set USBCMD.RS=1 and USBCMD.INTE=1 simultaneously
     */
    uint32_t usbcmd = xhci->op_regs->USBCMD;
    usbcmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    xhci->op_regs->USBCMD = usbcmd;
    __asm__ volatile("mfence" ::: "memory"); /* CRITICAL: Memory barrier */
    
    klog_printf(KLOG_INFO, "xhci: controller start command issued (RUN=1 INTE=1)");
    
    /* Wait for controller to start - USBSTS.HCH must become 0 */
    if (xhci_wait_for_bit(&xhci->op_regs->USBSTS, XHCI_STS_HCH, 0, 100000, "HCH clear") < 0) {
        uint32_t usbsts = xhci->op_regs->USBSTS;
        klog_printf(KLOG_ERROR, "xhci: ========================================");
        klog_printf(KLOG_ERROR, "xhci: CONTROLLER START TIMEOUT!");
        klog_printf(KLOG_ERROR, "xhci: ========================================");
        klog_printf(KLOG_ERROR, "xhci: USBSTS=0x%08x", usbsts);
        klog_printf(KLOG_ERROR, "xhci: HCH=%u CNR=%u HSE=%u EINT=%u",
                    (usbsts & XHCI_STS_HCH) ? 1 : 0,
                    (usbsts & XHCI_STS_CNR) ? 1 : 0,
                    (usbsts & XHCI_STS_HSE) ? 1 : 0,
                    (usbsts & XHCI_STS_EINT) ? 1 : 0);
        if (usbsts & (1 << 12)) {
            klog_printf(KLOG_ERROR, "xhci: Controller Error Detected!");
        }
        if (usbsts & XHCI_STS_HSE) {
            klog_printf(KLOG_ERROR, "xhci: Host System Error!");
        }
        return -1;
    }
    
    uint32_t usbsts_final = xhci->op_regs->USBSTS;
    klog_printf(KLOG_INFO, "xhci: controller started successfully!");
    klog_printf(KLOG_INFO, "xhci: USBSTS=0x%08x (HCH=%u CNR=%u HSE=%u EINT=%u)",
                usbsts_final,
                (usbsts_final & XHCI_STS_HCH) ? 1 : 0,
                (usbsts_final & XHCI_STS_CNR) ? 1 : 0,
                (usbsts_final & XHCI_STS_HSE) ? 1 : 0,
                (usbsts_final & XHCI_STS_EINT) ? 1 : 0);
    
    /* Set number of ports in host controller structure */
    hc->num_ports = xhci->num_ports;
    
    klog_printf(KLOG_INFO, "xhci: ========================================");
    klog_printf(KLOG_INFO, "xhci: Complete Initialization Sequence SUCCESS");
    klog_printf(KLOG_INFO, "xhci: ========================================");
    
    return 0;
}

/**
 * Post command to command ring and ring doorbell
 * 
 * XHCI 1.2 Spec Section 5.3.3: Command Ring Processing
 * 
 * @param xhci Controller
 * @param trb TRB to enqueue
 * @return 0 on success, -1 on error
 */
int xhci_post_command(xhci_controller_t *xhci, xhci_trb_t *trb) {
    if (!xhci || !trb) {
        klog_printf(KLOG_ERROR, "xhci: invalid parameters for post_command");
        return -1;
    }
    
    /* Enqueue TRB to command ring */
    extern int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb);
    if (xhci_cmd_ring_enqueue(&xhci->cmd_ring, trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci: failed to enqueue command TRB");
        return -1;
    }
    
    /* Ring command doorbell (slot 0, endpoint 0) - XHCI 1.2 Spec Section 5.5 */
    extern void xhci_ring_cmd_doorbell(xhci_controller_t *xhci);
    xhci_ring_cmd_doorbell(xhci);
    
    return 0;
}

/**
 * Poll event ring for new events
 * 
 * @param xhci Controller
 * @param event Output event TRB
 * @return 0 if event available, -1 if no event
 */
int xhci_poll_event(xhci_controller_t *xhci, xhci_event_trb_t *event) {
    if (!xhci || !event) {
        return -1;
    }
    
    /* Dequeue event from event ring */
    extern int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb, void *rt_regs);
    if (xhci_event_ring_dequeue(&xhci->event_ring, event, xhci->rt_regs) < 0) {
        return -1; /* No event available */
    }
    
    return 0;
}
