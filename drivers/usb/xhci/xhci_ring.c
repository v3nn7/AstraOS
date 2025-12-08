/**
 * XHCI Transfer Ring Management - Complete Fixed Version
 * 
 * Implements TRB ring allocation, enqueueing, and dequeueing.
 * Handles command ring, event ring, and transfer rings.
 * 
 * CRITICAL FIXES:
 * 1. ERDP EHB bit must be cleared after processing events
 * 2. Command ring cycle state never toggles in software
 * 3. Event ring dequeue now updates ERDP properly
 */

#include "usb/xhci.h"
#include "usb/xhci_internal.h"
#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "pmm.h"
#include "mmio.h"
#include "vmm.h"

/* offsetof macro for bare-metal kernel */
#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

/* Helper macros for MMIO access */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* Helper macros */
#define XHCI_TRB_SIZE           16
#define XHCI_RING_SIZE          256  /* 256 TRBs per ring */
#define XHCI_RING_MASK          (XHCI_RING_SIZE - 1)

/**
 * Allocate and initialize a command ring
 */
static int xhci_command_ring_alloc(xhci_command_ring_t *ring, uint32_t size) {
    if (!ring || size == 0) return -1;

    /* Round up to power of 2 */
    uint32_t ring_size = 1;
    while (ring_size < size) {
        ring_size <<= 1;
    }

    /* Allocate TRBs (must be 16-byte aligned) */
    size_t trb_array_size = ring_size * XHCI_TRB_SIZE;
    ring->trbs = (xhci_trb_t *)kmalloc(trb_array_size + 64); /* Extra for alignment */
    if (!ring->trbs) {
        klog_printf(KLOG_ERROR, "xhci_ring: allocation failed");
        return -1;
    }

    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ring->trbs;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ring->trbs = (xhci_trb_t *)addr;
    }

    k_memset(ring->trbs, 0, trb_array_size);

    ring->size = ring_size;
    ring->dequeue = 0;
    ring->enqueue = 0;
    ring->cycle_state = true; /* Command ring starts with cycle = 1 per XHCI spec */

    /* Get physical address from virtual address */
    uint64_t virt_addr = (uint64_t)(uintptr_t)ring->trbs;
    uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
    if (phys_addr == 0) {
        /* Fallback: use HHDM offset if available */
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
            phys_addr = virt_addr - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci_ring: failed to get physical address for command ring");
            kfree(ring->trbs);
            return -1;
        }
    }
    ring->phys_addr = (phys_addr_t)phys_addr;

    klog_printf(KLOG_INFO, "xhci_ring: allocated command ring size=%u at virt=%p phys=0x%016llx", 
                ring_size, ring->trbs, (unsigned long long)phys_addr);
    return 0;
}

/**
 * Allocate and initialize an event ring
 */
static int xhci_event_ring_alloc(xhci_event_ring_t *ring, uint32_t size) {
    if (!ring || size == 0) return -1;

    /* Round up to power of 2 */
    uint32_t ring_size = 1;
    while (ring_size < size) {
        ring_size <<= 1;
    }

    /* Allocate event TRBs */
    size_t trb_array_size = ring_size * sizeof(xhci_event_trb_t);
    ring->trbs = (xhci_event_trb_t *)kmalloc(trb_array_size + 64);
    if (!ring->trbs) {
        klog_printf(KLOG_ERROR, "xhci_ring: event ring allocation failed");
        return -1;
    }

    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ring->trbs;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ring->trbs = (xhci_event_trb_t *)addr;
    }

    k_memset(ring->trbs, 0, trb_array_size);

    ring->size = ring_size;
    ring->dequeue = 0;
    ring->enqueue = 0;
    ring->cycle_state = true; /* Event ring starts with cycle = 1 */

    /* Get physical address from virtual address */
    uint64_t virt_addr = (uint64_t)(uintptr_t)ring->trbs;
    uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
    if (phys_addr == 0) {
        /* Fallback: use HHDM offset if available */
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
            phys_addr = virt_addr - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci_ring: failed to get physical address for event ring");
            kfree(ring->trbs);
            return -1;
        }
    }
    ring->phys_addr = (phys_addr_t)phys_addr;
    ring->segment_table = NULL;
    ring->segment_table_phys = 0;

    klog_printf(KLOG_INFO, "xhci_ring: allocated event ring size=%u at virt=%p phys=0x%016llx", 
                ring_size, ring->trbs, (unsigned long long)phys_addr);
    return 0;
}

/**
 * Allocate and initialize a transfer ring
 */
static int xhci_transfer_ring_alloc(xhci_transfer_ring_t *ring, uint32_t size) {
    if (!ring || size == 0) return -1;

    /* Round up to power of 2 */
    uint32_t ring_size = 1;
    while (ring_size < size) {
        ring_size <<= 1;
    }

    /* Allocate TRBs */
    size_t trb_array_size = ring_size * XHCI_TRB_SIZE;
    ring->trbs = (xhci_trb_t *)kmalloc(trb_array_size + 64);
    if (!ring->trbs) {
        klog_printf(KLOG_ERROR, "xhci_ring: transfer ring allocation failed");
        return -1;
    }

    /* Align to 64-byte boundary */
    uintptr_t addr = (uintptr_t)ring->trbs;
    if (addr % 64 != 0) {
        addr = (addr + 63) & ~63;
        ring->trbs = (xhci_trb_t *)addr;
    }

    k_memset(ring->trbs, 0, trb_array_size);

    ring->size = ring_size;
    ring->dequeue = 0;
    ring->enqueue = 0;
    ring->cycle_state = true; /* Transfer ring starts with cycle = 1 */

    /* Get physical address from virtual address */
    uint64_t virt_addr = (uint64_t)(uintptr_t)ring->trbs;
    uint64_t phys_addr = vmm_virt_to_phys(virt_addr);
    if (phys_addr == 0) {
        /* Fallback: use HHDM offset if available */
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
            phys_addr = virt_addr - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci_ring: failed to get physical address for transfer ring");
            kfree(ring->trbs);
            return -1;
        }
    }
    ring->phys_addr = (phys_addr_t)phys_addr;

    klog_printf(KLOG_INFO, "xhci_ring: allocated transfer ring size=%u at virt=%p phys=0x%016llx", 
                ring_size, ring->trbs, (unsigned long long)phys_addr);
    return 0;
}

/**
 * Enqueue a TRB to command ring
 * 
 * CRITICAL FIX: cycle_state NEVER toggles for command rings!
 * The HC manages CSS internally when it processes the Link TRB with TC bit.
 * Software cycle_state remains constant at 1 (the initial value).
 */
int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb) {
    if (!ring || !trb || !ring->trbs) return -1;

    uint32_t index = ring->enqueue & XHCI_RING_MASK;

    /* Check if we're about to overwrite Link TRB */
    uint32_t link_trb_index = (ring->size - 1) & XHCI_RING_MASK;
    if (index == link_trb_index) {
        klog_printf(KLOG_ERROR, "xhci_ring: command ring full (would overwrite Link TRB at index %u)", index);
        return -1;
    }

    /* Copy TRB */
    memcpy(&ring->trbs[index], trb, sizeof(xhci_trb_t));

    /* CRITICAL: Set cycle bit to match current ring cycle_state
     * For command rings, this is ALWAYS 1 (never changes in software)
     */
    if (ring->cycle_state) {
        ring->trbs[index].control |= XHCI_TRB_CYCLE;
    } else {
        ring->trbs[index].control &= ~XHCI_TRB_CYCLE;
    }

    /* CRITICAL: Flush cache to ensure HC sees the TRB */
    extern void xhci_flush_cache(void *addr, size_t sz);
    xhci_flush_cache(&ring->trbs[index], sizeof(xhci_trb_t));

    klog_printf(KLOG_DEBUG, "xhci_ring: enqueued TRB at index=%u cycle=%u enqueue=%u type=%u",
                index, ring->cycle_state ? 1 : 0, ring->enqueue,
                (trb->control >> 10) & 0x3F);

    /* Memory barrier - CRITICAL: HC must see TRB before doorbell ring */
    __asm__ volatile("mfence" ::: "memory");

    /* Update enqueue pointer */
    ring->enqueue++;

    /* CRITICAL FIX: When enqueue wraps, DO NOT toggle cycle_state for command rings!
     * The HC manages CSS via the Link TRB's Toggle Cycle (TC) bit.
     * Software cycle_state stays constant at 1.
     */
    if ((ring->enqueue & XHCI_RING_MASK) == 0) {
        klog_printf(KLOG_DEBUG, "xhci_ring: enqueue wrapped to 0 (cycle_state remains %u)",
                    ring->cycle_state ? 1 : 0);
    }

    return 0;
}

/**
 * Enqueue a TRB to transfer ring
 */
int xhci_transfer_ring_enqueue(xhci_transfer_ring_t *ring, xhci_trb_t *trb) {
    if (!ring || !trb || !ring->trbs) return -1;

    uint32_t index = ring->enqueue & XHCI_RING_MASK;

    /* Copy TRB */
    memcpy(&ring->trbs[index], trb, sizeof(xhci_trb_t));

    /* Set cycle bit */
    if (ring->cycle_state) {
        ring->trbs[index].control |= XHCI_TRB_CYCLE;
    } else {
        ring->trbs[index].control &= ~XHCI_TRB_CYCLE;
    }

    /* Update enqueue pointer */
    ring->enqueue++;

    /* Toggle cycle bit if we wrapped around */
    if ((ring->enqueue & XHCI_RING_MASK) == 0) {
        ring->cycle_state = !ring->cycle_state;
    }

    /* Memory barrier */
    __asm__ volatile("mfence" ::: "memory");

    return 0;
}

/**
 * Dequeue an event TRB from event ring
 * 
 * CRITICAL FIX: Must update ERDP and clear EHB bit after each event!
 * Without this, the HC stops generating events after the first batch.
 */
int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb, void *rt_regs) {
    if (!ring || !trb || !ring->trbs || !rt_regs) return -1;

    uint32_t index = ring->dequeue & XHCI_RING_MASK;

    /* Check cycle bit - it's in the control field (bit 0) */
    uint32_t control = ((uint32_t *)&ring->trbs[index])[2]; /* Third 32-bit word */
    uint8_t cycle_bit = (control & 1) ? 1 : 0;

    if (cycle_bit != ring->cycle_state) {
        return -1; /* No new event available */
    }

    /* Copy TRB */
    memcpy(trb, &ring->trbs[index], sizeof(xhci_event_trb_t));

    /* Update dequeue pointer */
    ring->dequeue++;

    /* Toggle cycle bit if we wrapped around */
    if ((ring->dequeue & XHCI_RING_MASK) == 0) {
        ring->cycle_state = !ring->cycle_state;
    }

    /* CRITICAL FIX: Update ERDP and clear EHB bit
     * This tells the HC we've processed the event and it can continue.
     * Without this, the HC thinks we're still busy and won't generate new events.
     * 
     * Note: EHB (Event Handler Busy) bit must be cleared by writing ERDP with EHB=0.
     * We write the ERDP address WITHOUT the EHB bit to clear it.
     */
    uint64_t erdp_phys = ring->phys_addr + ((ring->dequeue & XHCI_RING_MASK) * sizeof(xhci_event_trb_t));
    uint64_t erdp_value = erdp_phys & ~XHCI_ERDP_EHB; /* Write address WITHOUT EHB bit to clear it */

    XHCI_WRITE64(rt_regs, XHCI_ERDP(0), erdp_value);
    
    /* Verify EHB is cleared */
    uint64_t erdp_read = XHCI_READ64(rt_regs, XHCI_ERDP(0));
    if (erdp_read & XHCI_ERDP_EHB) {
        klog_printf(KLOG_WARN, "xhci_ring: EHB bit still set after clearing attempt");
    }

    /* Memory barrier to ensure ERDP write completes */
    __asm__ volatile("mfence" ::: "memory");

    return 0;
}

/**
 * Initialize command ring
 * 
 * CRITICAL FIXES:
 * 1. Link TRB must have correct cycle bit matching ring cycle_state
 * 2. Link TRB must have TC (Toggle Cycle) bit set to toggle CSS when HC processes it
 * 3. CRCR[63:6] = physical address of first TRB (64-byte aligned)
 * 4. CRCR[0] = RCS (Ring Cycle State) = 1 (matches ring cycle_state)
 * 5. CSS (Command Ring Cycle State) must synchronize with RCS before commands work
 * 6. Cache flush must be performed before writing to CRCR
 */
int xhci_cmd_ring_init(xhci_controller_t *xhci) {
    if (!xhci) return -1;

    int ret = xhci_command_ring_alloc(&xhci->cmd_ring, XHCI_RING_SIZE);
    if (ret < 0) {
        return -1;
    }

    /* CRITICAL: Add Link TRB at the end of the ring pointing back to the beginning
     * Link TRB structure per XHCI 1.2 spec:
     * - parameter[63:4] = Physical address of next TRB (16-byte aligned, but we use 64-byte)
     * - parameter[3:0] = Reserved (must be zero)
     * - control[9:0] = TRB Type = 6 (Link TRB)
     * - control[1] = TC (Toggle Cycle) - set to 1 to toggle CSS when HC processes Link TRB
     * - control[0] = Cycle bit - must match current ring cycle_state (which is 1)
     */
    uint32_t last_index = (xhci->cmd_ring.size - 1) & XHCI_RING_MASK;
    xhci_trb_t *link_trb = &xhci->cmd_ring.trbs[last_index];

    /* Build Link TRB pointing to first TRB with Toggle Cycle (TC) bit set
     * TC=1 means: when HC processes this Link TRB, it will toggle CSS (Command Ring Cycle State)
     * This allows the command ring to wrap around correctly.
     */
    uint64_t first_trb_phys = xhci->cmd_ring.phys_addr;
    
    /* CRITICAL: Verify first TRB address is 64-byte aligned */
    if ((first_trb_phys & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci: first TRB NOT 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)first_trb_phys);
        return -1;
    }
    
    xhci_build_link_trb(link_trb, first_trb_phys, true); /* toggle_cycle = 1 */

    /* CRITICAL: Link TRB cycle bit must match ring cycle_state (which is 1)
     * The cycle bit tells the HC whether this TRB is valid.
     * Since ring cycle_state = 1, we set cycle = 1.
     */
    link_trb->control |= XHCI_TRB_CYCLE;

    /* CRITICAL: Flush cache for Link TRB before HC reads it */
    extern void xhci_flush_cache(void *addr, size_t sz);
    xhci_flush_cache(link_trb, sizeof(xhci_trb_t));

    /* Memory barrier to ensure Link TRB is written */
    __asm__ volatile("mfence" ::: "memory");

    klog_printf(KLOG_INFO, "xhci: command ring initialized: size=%u link_trb[%u] points to 0x%016llx cycle=%u TC=%u",
                xhci->cmd_ring.size, last_index, (unsigned long long)first_trb_phys,
                xhci->cmd_ring.cycle_state ? 1 : 0,
                (link_trb->control & XHCI_TRB_TC) ? 1 : 0);

    /* CRITICAL: Per XHCI 1.2 spec, controller MUST be stopped before writing CRCR
     * Step 1: Stop the controller (clear RUNSTOP bit)
     */
    uint32_t usbcmd_before = xhci->op_regs->USBCMD;
    uint32_t usbsts_before = xhci->op_regs->USBSTS;
    
    klog_printf(KLOG_INFO, "xhci: stopping controller before CRCR write: USBCMD=0x%08x USBSTS=0x%08x",
                usbcmd_before, usbsts_before);
    
    /* Clear RUNSTOP bit (bit 0) */
    uint32_t usbcmd = usbcmd_before;
    usbcmd &= ~XHCI_CMD_RUN;
    xhci->op_regs->USBCMD = usbcmd;
    __asm__ volatile("mfence" ::: "memory");
    
    /* Wait until controller is halted (USBSTS.HCH == 1) */
    uint32_t halt_timeout = 2000;
    uint32_t halt_loops = 0;
    while (halt_loops < halt_timeout) {
        uint32_t usbsts = xhci->op_regs->USBSTS;
        if (usbsts & XHCI_STS_HCH) {
            klog_printf(KLOG_INFO, "xhci: controller halted (USBSTS.HCH=1) after %u loops", halt_loops);
            break;
        }
        for (volatile int i = 0; i < 1000; i++);
        halt_loops++;
    }
    
    if (halt_loops >= halt_timeout) {
        uint32_t usbsts = xhci->op_regs->USBSTS;
        klog_printf(KLOG_ERROR, "xhci: controller halt timeout! USBSTS=0x%08x (HCH=%u)",
                    usbsts, (usbsts & XHCI_STS_HCH) ? 1 : 0);
        return -1;
    }
    
    uint32_t usbsts_after_halt = xhci->op_regs->USBSTS;
    klog_printf(KLOG_INFO, "xhci: controller halted: USBSTS=0x%08x (HCH=%u)",
                usbsts_after_halt, (usbsts_after_halt & XHCI_STS_HCH) ? 1 : 0);

    /* Set command ring pointer in operational registers */
    uint64_t cmd_ring_addr = xhci->cmd_ring.phys_addr;

    /* Verify 64-byte alignment */
    if ((cmd_ring_addr & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci: command ring NOT 64-byte aligned! phys=0x%016llx", 
                    (unsigned long long)cmd_ring_addr);
        return -1;
    }

    /* CRITICAL: Per XHCI 1.2 spec, before writing new CRCR:
     * Step 2: Clear CRCR.CRR (bit 3) and CRCR.CSS (bit 1) by writing zeros
     * This ensures any previous ring is fully stopped
     */
    uint64_t crcr_before = xhci->op_regs->CRCR;
    klog_printf(KLOG_INFO, "xhci: CRCR before clearing: 0x%016llx (RCS=%u CSS=%u CRR=%u CA=%u)",
                (unsigned long long)crcr_before,
                (crcr_before & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr_before & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr_before & XHCI_CRCR_CRR) ? 1 : 0,
                (crcr_before & XHCI_CRCR_CA) ? 1 : 0);
    
    /* Clear CRCR: set CRR=0, CSS=0, CA=0, but preserve address bits [63:6] */
    uint64_t crcr_cleared = (crcr_before & ~0x3F) & ~(XHCI_CRCR_CRR | XHCI_CRCR_CSS | XHCI_CRCR_CA | XHCI_CRCR_RCS);
    xhci->op_regs->CRCR = crcr_cleared;
    __asm__ volatile("mfence" ::: "memory");
    
    /* Verify CRCR is cleared */
    uint64_t crcr_after_clear = xhci->op_regs->CRCR;
    klog_printf(KLOG_INFO, "xhci: CRCR after clearing: 0x%016llx (RCS=%u CSS=%u CRR=%u CA=%u)",
                (unsigned long long)crcr_after_clear,
                (crcr_after_clear & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr_after_clear & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr_after_clear & XHCI_CRCR_CRR) ? 1 : 0,
                (crcr_after_clear & XHCI_CRCR_CA) ? 1 : 0);

    /* CRITICAL: Flush cache for command ring before writing CRCR
     * The HC must see the command ring (including Link TRB) in memory
     */
    size_t cmd_ring_size = xhci->cmd_ring.size * sizeof(xhci_trb_t);
    xhci_flush_cache(xhci->cmd_ring.trbs, cmd_ring_size);

    /* CRITICAL: Per XHCI 1.2 spec:
     * Step 3: Write new command ring pointer
     * [63:6] = Command Ring Pointer (physical address, 64-byte aligned)
     * [0] = RCS (Ring Cycle State) = 1 (matches ring cycle_state)
     * DO NOT set CSS bit - HC will set it automatically
     * DO NOT set CRR bit - HC will set it when processing commands
     */
    uint64_t crcr_value = (cmd_ring_addr & ~0x3F) | XHCI_CRCR_RCS; /* Clear bits [5:0] except RCS */

    /* Write CRCR */
    xhci->op_regs->CRCR = crcr_value;

    /* CRITICAL: Memory barrier to ensure CRCR write is visible to HC */
    __asm__ volatile("mfence" ::: "memory");

    /* Read CRCR back for verification */
    uint64_t crcr_read = xhci->op_regs->CRCR;
    uint8_t rcs_bit = (crcr_read & XHCI_CRCR_RCS) ? 1 : 0;
    uint8_t css_bit = (crcr_read & XHCI_CRCR_CSS) ? 1 : 0;
    uint8_t crr_bit = (crcr_read & XHCI_CRCR_CRR) ? 1 : 0;
    uint8_t ca_bit = (crcr_read & XHCI_CRCR_CA) ? 1 : 0;

    klog_printf(KLOG_INFO, "xhci: structure offsets verification:");
    klog_printf(KLOG_INFO, "  sizeof(xhci_op_regs_t) = %zu", sizeof(xhci_op_regs_t));
    klog_printf(KLOG_INFO, "  CRCR offset = 0x%zx (expected 0x18)", offsetof(xhci_op_regs_t, CRCR));
    klog_printf(KLOG_INFO, "  USBCMD offset = 0x%zx (expected 0x00)", offsetof(xhci_op_regs_t, USBCMD));
    klog_printf(KLOG_INFO, "  USBSTS offset = 0x%zx (expected 0x04)", offsetof(xhci_op_regs_t, USBSTS));
    klog_printf(KLOG_INFO, "  DNCTRL offset = 0x%zx (expected 0x10)", offsetof(xhci_op_regs_t, DNCTRL));
    klog_printf(KLOG_INFO, "  DCBAAP offset = 0x%zx (expected 0x30)", offsetof(xhci_op_regs_t, DCBAAP));

    klog_printf(KLOG_INFO, "xhci: command ring initialized:");
    klog_printf(KLOG_INFO, "  virt=%p phys=0x%016llx (aligned=%d)",
                xhci->cmd_ring.trbs, (unsigned long long)cmd_ring_addr,
                ((cmd_ring_addr & 0x3F) == 0) ? 1 : 0);
    klog_printf(KLOG_INFO, "  CRCR_write=0x%016llx CRCR_read=0x%016llx", 
                (unsigned long long)crcr_value, (unsigned long long)crcr_read);
    klog_printf(KLOG_INFO, "  RCS=%u CSS=%u CRR=%u CA=%u (immediately after write)", 
                rcs_bit, css_bit, crr_bit, ca_bit);

    /* CRITICAL: Per XHCI 1.2 spec, after writing CRCR:
     * Step 4: HC SHOULD automatically set CSS=1 to match RCS=1
     * CSS (Command Ring Cycle State) is managed by the HC and must match RCS
     * before commands can be processed. If CSS != RCS, the HC won't process commands.
     * 
     * The HC synchronizes CSS to RCS when CRCR is written, but this may take time.
     * We wait up to 100ms for CSS to synchronize.
     */
    uint32_t sync_timeout = 100000; /* 100ms in microseconds */
    uint32_t sync_loops = 0;
    while (sync_loops < sync_timeout) {
        crcr_read = xhci->op_regs->CRCR;
        rcs_bit = (crcr_read & XHCI_CRCR_RCS) ? 1 : 0;
        css_bit = (crcr_read & XHCI_CRCR_CSS) ? 1 : 0;
        crr_bit = (crcr_read & XHCI_CRCR_CRR) ? 1 : 0;
        
        if (css_bit == rcs_bit && css_bit == 1) {
            klog_printf(KLOG_INFO, "xhci: CRCR CSS synchronized with RCS (RCS=%u CSS=%u) after %u loops",
                        rcs_bit, css_bit, sync_loops);
            break;
        }
        
        for (volatile int i = 0; i < 100; i++); /* Small delay */
        sync_loops += 100;
    }
    
    /* Final read after sync attempt */
    crcr_read = xhci->op_regs->CRCR;
    rcs_bit = (crcr_read & XHCI_CRCR_RCS) ? 1 : 0;
    css_bit = (crcr_read & XHCI_CRCR_CSS) ? 1 : 0;
    crr_bit = (crcr_read & XHCI_CRCR_CRR) ? 1 : 0;
    ca_bit = (crcr_read & XHCI_CRCR_CA) ? 1 : 0;
    
    if (sync_loops >= sync_timeout) {
        klog_printf(KLOG_ERROR, "xhci: CRCR CSS synchronization timeout!");
        klog_printf(KLOG_ERROR, "xhci: CRCR final state: 0x%016llx RCS=%u CSS=%u CRR=%u CA=%u",
                    (unsigned long long)crcr_read, rcs_bit, css_bit, crr_bit, ca_bit);
        klog_printf(KLOG_ERROR, "xhci: Command ring will NOT work until CSS synchronizes!");
        klog_printf(KLOG_ERROR, "xhci: This indicates incorrect initialization sequence!");
        return -1; /* Return error - CSS must be 1 for command ring to work */
    }
    
    klog_printf(KLOG_INFO, "xhci: CRCR final state: 0x%016llx RCS=%u CSS=%u CRR=%u CA=%u", 
                (unsigned long long)crcr_read, rcs_bit, css_bit, crr_bit, ca_bit);

    klog_printf(KLOG_INFO, "  ring cycle_state=%u enqueue=%u", 
                xhci->cmd_ring.cycle_state ? 1 : 0, xhci->cmd_ring.enqueue);

    klog_printf(KLOG_INFO, "xhci: command ring ready (CSS synchronized)");

    return 0;
}

/**
 * Initialize event ring
 * 
 * CRITICAL FIXES:
 * 1. ERST must be 64-byte aligned (per XHCI 1.2 spec)
 * 2. ERST entry structure: base address (64-bit) + size (32-bit) + reserved (32-bit)
 * 3. ERDP must point to first event TRB (64-byte aligned)
 * 4. EHB bit must be cleared during initialization
 * 5. Cache flush must be performed before writing to ERSTBA/ERSTSZ/ERDP
 */
int xhci_event_ring_init(xhci_controller_t *xhci) {
    if (!xhci) return -1;

    int ret = xhci_event_ring_alloc(&xhci->event_ring, XHCI_RING_SIZE);
    if (ret < 0) {
        return -1;
    }

    /* CRITICAL FIX: Allocate ERST with 64-byte alignment
     * ERST (Event Ring Segment Table) must be 64-byte aligned per XHCI 1.2 spec
     */
    size_t erst_size = sizeof(xhci_erst_entry_t) * 1; /* Single segment for now */
    void *erst_alloc = kmalloc(erst_size + 64); /* Extra for alignment */
    if (!erst_alloc) {
        kfree(xhci->event_ring.trbs);
        return -1;
    }

    /* Align to 64-byte boundary */
    uintptr_t erst_addr = (uintptr_t)erst_alloc;
    if (erst_addr % 64 != 0) {
        erst_addr = (erst_addr + 63) & ~63;
    }
    xhci_erst_entry_t *erst = (xhci_erst_entry_t *)erst_addr;

    k_memset(erst, 0, erst_size);

    /* CRITICAL: Verify event ring physical address is 64-byte aligned */
    uint64_t event_ring_addr = xhci->event_ring.phys_addr;
    if ((event_ring_addr & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci_ring: event ring NOT 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)event_ring_addr);
        kfree(erst_alloc);
        kfree(xhci->event_ring.trbs);
        return -1;
    }

    /* Set up ERST entry
     * ERST entry structure per XHCI 1.2 spec:
     * - ring_segment_base (64-bit): Physical address of event ring segment (64-byte aligned)
     * - ring_segment_size (32-bit): Number of TRBs in segment (must be power of 2)
     * - reserved (32-bit): Must be zero
     */
    erst[0].ring_segment_base = event_ring_addr;
    erst[0].ring_segment_size = XHCI_RING_SIZE;
    erst[0].reserved = 0; /* CRITICAL: Must be zero */

    xhci->event_ring.segment_table = erst;

    /* Get physical address of ERST segment table */
    uint64_t erst_virt = (uint64_t)(uintptr_t)erst;
    uint64_t erst_phys = vmm_virt_to_phys(erst_virt);
    if (erst_phys == 0) {
        /* Fallback: use HHDM offset if available */
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && erst_virt >= pmm_hhdm_offset) {
            erst_phys = erst_virt - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci_ring: failed to get physical address for ERST");
            kfree(erst_alloc);
            kfree(xhci->event_ring.trbs);
            return -1;
        }
    }

    /* CRITICAL: Verify ERST physical address is 64-byte aligned */
    if ((erst_phys & 0x3F) != 0) {
        klog_printf(KLOG_ERROR, "xhci_ring: ERST NOT 64-byte aligned! phys=0x%016llx",
                    (unsigned long long)erst_phys);
        kfree(erst_alloc);
        kfree(xhci->event_ring.trbs);
        return -1;
    }

    xhci->event_ring.segment_table_phys = (phys_addr_t)erst_phys;

    /* CRITICAL: Flush cache for ERST before writing to registers
     * The host controller must see the ERST structure in memory
     */
    extern void xhci_flush_cache(void *addr, size_t sz);
    xhci_flush_cache(erst, erst_size);

    /* CRITICAL: Set ERST pointer in runtime registers
     * ERSTBA[63:6] = Physical address of ERST (64-byte aligned)
     * ERSTBA[5:0] = Reserved (must be zero)
     */
    XHCI_WRITE64(xhci->rt_regs, XHCI_ERSTBA(0), erst_phys);
    XHCI_WRITE32(xhci->rt_regs, XHCI_ERSTSZ(0), 1); /* 1 segment */

    /* CRITICAL: Set event ring dequeue pointer
     * ERDP[63:4] = Physical address of first event TRB (16-byte aligned, but we use 64-byte)
     * ERDP[3] = EHB (Event Handler Busy) - must be 0 during initialization
     * ERDP[2:0] = Reserved (must be zero)
     * 
     * ERDP must point to the first event TRB in the event ring.
     * The HC will set EHB=1 when processing events, and we clear it by writing ERDP with EHB=0.
     */
    uint64_t erdp_value = event_ring_addr & ~XHCI_ERDP_EHB; /* Clear EHB bit */
    XHCI_WRITE64(xhci->rt_regs, XHCI_ERDP(0), erdp_value);

    /* CRITICAL: Memory barrier to ensure all writes are visible to HC */
    __asm__ volatile("mfence" ::: "memory");

    /* Verify Event Ring setup */
    uint64_t erstba_read = XHCI_READ64(xhci->rt_regs, XHCI_ERSTBA(0));
    uint32_t erstsz_read = XHCI_READ32(xhci->rt_regs, XHCI_ERSTSZ(0));
    uint64_t erdp_read = XHCI_READ64(xhci->rt_regs, XHCI_ERDP(0));

    klog_printf(KLOG_INFO, "xhci: event ring initialized:");
    klog_printf(KLOG_INFO, "  event_ring: virt=%p phys=0x%016llx (aligned=%d)",
                xhci->event_ring.trbs, (unsigned long long)event_ring_addr,
                ((event_ring_addr & 0x3F) == 0) ? 1 : 0);
    klog_printf(KLOG_INFO, "  ERST: virt=%p phys=0x%016llx (aligned=%d)",
                erst, (unsigned long long)erst_phys,
                ((erst_phys & 0x3F) == 0) ? 1 : 0);
    klog_printf(KLOG_INFO, "  ERST entry: base=0x%016llx size=%u reserved=%u",
                (unsigned long long)erst[0].ring_segment_base,
                erst[0].ring_segment_size, erst[0].reserved);
    klog_printf(KLOG_INFO, "  ERSTBA=0x%016llx ERSTSZ=%u ERDP=0x%016llx (EHB=%u)",
                (unsigned long long)erstba_read, erstsz_read, (unsigned long long)erdp_read,
                (erdp_read & XHCI_ERDP_EHB) ? 1 : 0);

    /* CRITICAL: Verify ERDP EHB is cleared */
    if (erdp_read & XHCI_ERDP_EHB) {
        klog_printf(KLOG_WARN, "xhci_ring: ERDP EHB bit is set during initialization!");
    }

    klog_printf(KLOG_INFO, "xhci: event ring ready");

    return 0;
}

/**
 * Initialize transfer ring for an endpoint
 */
int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot >= 32 || endpoint >= 32) return -1;

    if (xhci->transfer_rings[slot][endpoint]) {
        /* Already initialized */
        return 0;
    }

    xhci_transfer_ring_t *ring = (xhci_transfer_ring_t *)kmalloc(sizeof(xhci_transfer_ring_t));
    if (!ring) {
        return -1;
    }

    k_memset(ring, 0, sizeof(xhci_transfer_ring_t));

    int ret = xhci_transfer_ring_alloc(ring, XHCI_RING_SIZE);
    if (ret < 0) {
        kfree(ring);
        return -1;
    }

    /* Add Link TRB at the end of the transfer ring */
    uint32_t last_index = (ring->size - 1) & XHCI_RING_MASK;
    xhci_trb_t *link_trb = &ring->trbs[last_index];

    /* Build Link TRB pointing to first TRB with Toggle Cycle (TC) bit set */
    uint64_t first_trb_phys = ring->phys_addr;
    xhci_build_link_trb(link_trb, first_trb_phys, true); /* toggle_cycle = 1 */

    /* Start cycle=1: set cycle bit on Link TRB */
    link_trb->control |= XHCI_TRB_CYCLE;

    /* Memory barrier to ensure Link TRB is written */
    __asm__ volatile("mfence" ::: "memory");

    xhci->transfer_rings[slot][endpoint] = ring;

    klog_printf(KLOG_INFO, "xhci: transfer ring slot=%u ep=%u initialized: size=%u link_trb[%u] points to 0x%016llx",
                slot, endpoint, ring->size, last_index, (unsigned long long)first_trb_phys);
    return 0;
}

/**
 * Free transfer ring for an endpoint
 */
void xhci_transfer_ring_free(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint) {
    if (!xhci || slot >= 32 || endpoint >= 32) return;

    if (xhci->transfer_rings[slot][endpoint]) {
        xhci_transfer_ring_t *ring = xhci->transfer_rings[slot][endpoint];
        if (ring->trbs) {
            kfree(ring->trbs);
        }
        kfree(ring);
        xhci->transfer_rings[slot][endpoint] = NULL;
    }
}

/**
 * Build a TRB from parameters
 */
void xhci_build_trb(xhci_trb_t *trb, uint64_t data_ptr, uint32_t length, uint32_t type, uint32_t flags) {
    if (!trb) return;

    k_memset(trb, 0, sizeof(xhci_trb_t));

    trb->parameter = data_ptr;
    trb->status = length | (flags << 16);
    trb->control = type | (1 << 10); /* IOC bit */
}

/**
 * Build a Link TRB
 */
void xhci_build_link_trb(xhci_trb_t *trb, uint64_t next_ring_addr, bool toggle_cycle) {
    if (!trb) return;

    k_memset(trb, 0, sizeof(xhci_trb_t));

    trb->parameter = next_ring_addr;
    trb->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | 
                   (toggle_cycle ? XHCI_TRB_TC : 0);
}