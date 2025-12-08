/**
 * XHCI Device Management
 * 
 * Enable Slot and Address Device commands.
 * These are required before any transfers can work.
 */

#include "usb/xhci.h"
#include "usb/xhci_internal.h"
#include "usb/xhci_context.h"
#include "usb/usb_core.h"
#include "kmalloc.h"
#include "kernel.h"
#include "klog.h"
#include "string.h"
#include "mmio.h"
#include "vmm.h"
#include "usb/util/usb_debug.h"

extern void xhci_dump_trb_level(int level, const char *msg, xhci_trb_t *trb);

/* Context Entry Masks are now in xhci_context.h */

/* Endpoint States (from xhci_context.c) */
#define XHCI_EP_STATE_DISABLED   0
#define XHCI_EP_STATE_RUNNING   1
#define XHCI_EP_STATE_HALTED    2
#define XHCI_EP_STATE_STOPPED   3
#define XHCI_EP_STATE_ERROR     4

/* Forward declarations */
extern int xhci_cmd_ring_enqueue(xhci_command_ring_t *ring, xhci_trb_t *trb);
extern void xhci_ring_cmd_doorbell(xhci_controller_t *xhci);
extern int xhci_process_events(xhci_controller_t *xhci);
extern int xhci_transfer_ring_init(xhci_controller_t *xhci, uint32_t slot, uint32_t endpoint);
extern int xhci_event_ring_dequeue(xhci_event_ring_t *ring, xhci_event_trb_t *trb, void *rt_regs);

/* Helper macros */
#define XHCI_READ32(regs, offset) mmio_read32((volatile uint32_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE32(regs, offset, val) mmio_write32((volatile uint32_t *)((uintptr_t)(regs) + (offset)), val)
#define XHCI_READ64(regs, offset) mmio_read64((volatile uint64_t *)((uintptr_t)(regs) + (offset)))
#define XHCI_WRITE64(regs, offset, val) mmio_write64((volatile uint64_t *)((uintptr_t)(regs) + (offset)), val)

/* Event TRB type for Command Completion (per XHCI spec) */
#ifndef XHCI_TRB_TYPE_COMMAND_COMPLETION
#define XHCI_TRB_TYPE_COMMAND_COMPLETION 33
#endif

/* Command Completion Event Tracking */
static uint32_t g_cmd_slot_id = 0;
static bool g_cmd_complete = false;
static uint8_t g_cmd_completion_code = 0;

/**
 * Process command completion events
 * Called from xhci_process_events() when command completion event is detected
 */
void xhci_handle_command_completion(xhci_controller_t *xhci, xhci_event_trb_t *event) {
    (void)xhci;  // Mark as unused for now
    uint8_t event_type = (event->control >> XHCI_EVENT_TRB_TYPE_SHIFT) & XHCI_EVENT_TRB_TYPE_MASK;
    
    klog_printf(KLOG_DEBUG, "xhci_device: handle_command_completion called: event_type=%u", event_type);
    
    if (event_type == 33) { /* Command Completion Event */
        uint8_t completion_code = event->status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
        uint64_t cmd_trb_ptr = event->data;
        uint8_t slot_id = (event->control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
        
        klog_printf(KLOG_INFO, "xhci_device: Command Completion Event: code=%u slot_id=%u cmd_ptr=0x%016llx",
                    completion_code, slot_id, (unsigned long long)cmd_trb_ptr);
        
        g_cmd_complete = true;
        g_cmd_completion_code = completion_code;
        
        /* For Enable Slot command, slot ID is returned in completion event */
        if (completion_code == XHCI_TRB_CC_SUCCESS) {
            /* Slot ID is in the slot_id field of the event TRB for Enable Slot */
            if (slot_id > 0) {
                g_cmd_slot_id = slot_id;
            } else {
                /* Fallback: try to extract from command TRB pointer */
                g_cmd_slot_id = (uint32_t)(cmd_trb_ptr & 0xFF);
            }
            klog_printf(KLOG_INFO, "xhci_device: command SUCCESS: slot_id=%u", g_cmd_slot_id);
        } else {
            klog_printf(KLOG_ERROR, "xhci_device: command FAILED: code=%u", completion_code);
        }
    } else {
        klog_printf(KLOG_DEBUG, "xhci_device: non-command event type=%u", event_type);
    }
}

/**
 * Wait for command completion with timeout (microsecond-based loops)
 * Uses event ring dequeue with rt_regs to clear EHB properly.
 */
static int xhci_wait_for_command_completion(xhci_controller_t *xhci,
                                            xhci_event_trb_t *event_out,
                                            uint32_t timeout_us) {
    uint32_t loops = 0;
    uint32_t events_processed = 0;
    uint32_t max_loops = timeout_us; /* Approximate microsecond timing */
    
    /* FIRST: Check if interrupts are enabled */
    uint32_t iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    klog_printf(KLOG_DEBUG, "xhci: waiting for command, IMAN=0x%08x (IE=%d, IP=%d)", 
                iman, (iman & (1 << 1)) ? 1 : 0, (iman & (1 << 0)) ? 1 : 0);
    
    if (!(iman & (1 << 1))) {
        klog_printf(KLOG_WARN, "xhci: interrupts are NOT enabled in IMAN!");
        /* Try to enable them */
        iman |= (1 << 1); /* IE bit */
        XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman);
        __asm__ volatile("mfence" ::: "memory");
    }
    
    /* SECOND: Clear any pending interrupts BEFORE waiting for command
     * IP (Interrupt Pending) is a "write 1 to clear" bit
     * We must clear it before waiting, otherwise interrupts may be blocked
     */
    iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0)); /* Read current value */
    XHCI_WRITE32(xhci->rt_regs, XHCI_IMAN(0), iman | (1 << 0)); /* Clear IP by writing 1 */
    __asm__ volatile("mfence" ::: "memory");
    
    /* Verify IP is cleared */
    uint32_t iman_after = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    if (iman_after & (1 << 0)) {
        klog_printf(KLOG_WARN, "xhci: IMAN.IP still set after clearing attempt");
    } else {
        klog_printf(KLOG_DEBUG, "xhci: IMAN.IP cleared successfully");
    }
    
    while (loops++ < max_loops) {
        /* Check USBSTS for errors first */
        uint32_t usbsts = xhci->op_regs->USBSTS;
        if (usbsts & XHCI_STS_HSE) {
            klog_printf(KLOG_ERROR, "xhci: Host System Error detected! USBSTS=0x%08x", usbsts);
            return -1;
        }
        
        /* Poll for command completion event */
        int ret = xhci_process_events(xhci);
        if (ret > 0) {
            events_processed += ret;
            
            /* Check if our command completed via global flags */
            if (g_cmd_complete) {
                klog_printf(KLOG_DEBUG, "xhci: command completed after %u loops, %u events",
                            loops, events_processed);
                if (event_out && g_cmd_completion_code == XHCI_TRB_CC_SUCCESS) {
                    /* Build a minimal event TRB for caller */
                    k_memset(event_out, 0, sizeof(xhci_event_trb_t));
                    event_out->status = g_cmd_completion_code;
                    event_out->control = (g_cmd_slot_id & 0xFF) << XHCI_EVENT_TRB_SLOT_ID_SHIFT;
                    event_out->control |= (33 << XHCI_EVENT_TRB_TYPE_SHIFT); /* Command Completion */
                    event_out->control |= 1; /* Cycle bit */
                }
                return 0;
            }
        }
        
        /* Small delay to avoid tight spin */
        if (loops % 100 == 0) {
            /* Check USBSTS again */
            usbsts = xhci->op_regs->USBSTS;
            uint32_t new_iman = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
            
            if (usbsts & XHCI_STS_EINT) {
                klog_printf(KLOG_DEBUG, "xhci: USBSTS shows interrupt pending (EINT=1)");
            }
            if (new_iman & (1 << 0)) {
                klog_printf(KLOG_DEBUG, "xhci: IMAN shows interrupt pending (IP=1)");
            }
            
            for (volatile int i = 0; i < 100; i++);
        }
    }
    
    /* TIMEOUT - gather debug info */
    klog_printf(KLOG_ERROR, "xhci: command timeout after %u loops, %u events processed",
                loops, events_processed);
    
    /* Debug: CRCR and ERDP */
    uint64_t crcr = xhci->op_regs->CRCR;
    uint64_t erdp = XHCI_READ64(xhci->rt_regs, XHCI_ERDP(0));
    uint32_t usbsts = xhci->op_regs->USBSTS;
    uint32_t iman_timeout = XHCI_READ32(xhci->rt_regs, XHCI_IMAN(0));
    
    klog_printf(KLOG_ERROR, "xhci: CRCR after timeout: 0x%016llx RCS=%u CSS=%u CRR=%u",
                (unsigned long long)crcr,
                (crcr & XHCI_CRCR_RCS) ? 1 : 0,
                (crcr & XHCI_CRCR_CSS) ? 1 : 0,
                (crcr & XHCI_CRCR_CRR) ? 1 : 0);
    klog_printf(KLOG_ERROR, "xhci: ERDP=0x%016llx EHB=%u",
                (unsigned long long)erdp,
                (erdp & XHCI_ERDP_EHB) ? 1 : 0);
    klog_printf(KLOG_ERROR, "xhci: USBSTS=0x%08x (HCH=%d, CNR=%d, HSE=%d, EINT=%d)",
                usbsts,
                (usbsts & XHCI_STS_HCH) ? 1 : 0,
                (usbsts & XHCI_STS_CNR) ? 1 : 0,
                (usbsts & XHCI_STS_HSE) ? 1 : 0,
                (usbsts & XHCI_STS_EINT) ? 1 : 0);
    klog_printf(KLOG_ERROR, "xhci: IMAN after timeout: 0x%08x (IE=%d, IP=%d)",
                iman_timeout,
                (iman_timeout & (1 << 1)) ? 1 : 0,
                (iman_timeout & (1 << 0)) ? 1 : 0);
    
    if (xhci->cmd_ring.trbs) {
        xhci_trb_t *first_trb = &xhci->cmd_ring.trbs[0];
        xhci_dump_trb_level(KLOG_ERROR, "first TRB (timeout)", first_trb);
    }
    
    return -1;
}

/* Device Context storage structure */
typedef struct {
    xhci_input_context_t *input_ctx;
    xhci_device_context_t *output_ctx;  /* Device Context from xhci_context.h */
    uint64_t output_ctx_phys;
    xhci_transfer_ring_t *ep0_ring;
} xhci_device_contexts_t;

/**
 * Prepare Input Context and Output Context for USB 3.0 device
 * For Enable Slot: Input Context contains ONLY Slot Context (no EP0 Context)
 * For Address Device: Input Context contains Slot Context + EP0 Context
 * Also allocates Output Context (Device Context) for DCBAAP[slot_id]
 * 
 * @param include_ep0 If true, include EP0 Context (for Address Device). If false, only Slot Context (for Enable Slot).
 */
static int xhci_prepare_input_context(xhci_controller_t *xhci, usb_device_t *dev, uint8_t speed, bool include_ep0) {
    if (!xhci || !dev) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid parameters for prepare_input_context");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci_device: preparing Input Context and Output Context for speed=%u", speed);
    
    /* Allocate Input Context (64-byte aligned) */
    extern xhci_input_context_t *xhci_input_context_alloc(void);
    xhci_input_context_t *input_ctx = xhci_input_context_alloc();
    if (!input_ctx) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to allocate Input Context");
        return -1;
    }
    
    /* Zero the entire context */
    k_memset(input_ctx, 0, sizeof(xhci_input_context_t));
    
    /* Allocate Output Context (Device Context) - must be 64-byte aligned */
    size_t output_ctx_size = sizeof(xhci_device_context_t);
    void *output_ctx_alloc = kmalloc(output_ctx_size + 64); /* Extra for alignment */
    if (!output_ctx_alloc) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to allocate Output Context");
        extern void xhci_input_context_free(xhci_input_context_t *ctx);
        xhci_input_context_free(input_ctx);
        return -1;
    }
    
    /* Align to 64-byte boundary */
    uintptr_t output_addr = (uintptr_t)output_ctx_alloc;
    if (output_addr % 64 != 0) {
        output_addr = (output_addr + 63) & ~63;
    }
    xhci_device_context_t *output_ctx = (xhci_device_context_t *)output_addr;
    k_memset(output_ctx, 0, output_ctx_size);
    
    /* Setup Slot Context using helper function */
    extern void xhci_input_context_set_slot(xhci_input_context_t *ctx, uint8_t slot_id, uint8_t root_port,
                                            uint8_t speed, uint8_t address, bool hub, uint8_t parent_slot,
                                            uint8_t parent_port);
    /* For Enable Slot, we don't have slot_id yet, so we use 0 temporarily */
    xhci_input_context_set_slot(input_ctx, 0, dev->port, speed, 0, false, 0, 0);
    
    xhci_transfer_ring_t *ep0_ring = NULL;
    uint64_t phys_addr = 0;
    
    /* CRITICAL: For Enable Slot, Input Context should contain ONLY Slot Context (no EP0 Context)
     * EP0 Context will be added in Address Device command.
     * For Address Device, we need EP0 Context, so allocate and set it up.
     */
    if (include_ep0) {
        /* Allocate transfer ring for EP0 */
        ep0_ring = (xhci_transfer_ring_t *)kmalloc(sizeof(xhci_transfer_ring_t));
        if (!ep0_ring) {
            klog_printf(KLOG_ERROR, "xhci_device: failed to allocate EP0 ring structure");
            extern void xhci_input_context_free(xhci_input_context_t *ctx);
            xhci_input_context_free(input_ctx);
            return -1;
        }
        k_memset(ep0_ring, 0, sizeof(xhci_transfer_ring_t));
        
        /* Allocate the ring buffer */
        size_t ring_size = 256; /* Default ring size */
        size_t trb_array_size = ring_size * 16; /* 16 bytes per TRB */
        ep0_ring->trbs = (xhci_trb_t *)kmalloc(trb_array_size + 64); /* Extra for alignment */
        if (!ep0_ring->trbs) {
            klog_printf(KLOG_ERROR, "xhci_device: failed to allocate EP0 ring buffer");
            kfree(ep0_ring);
            xhci_input_context_free(input_ctx);
            return -1;
        }
        
        /* Align to 64-byte boundary */
        uintptr_t addr = (uintptr_t)ep0_ring->trbs;
        if (addr % 64 != 0) {
            addr = (addr + 63) & ~63;
            ep0_ring->trbs = (xhci_trb_t *)addr;
        }
        
        k_memset(ep0_ring->trbs, 0, trb_array_size);
        ep0_ring->size = ring_size;
        ep0_ring->dequeue = 0;
        ep0_ring->enqueue = 0;
        ep0_ring->cycle_state = true; /* Start with cycle = 1 */
        
        /* Get physical address */
        uint64_t virt_addr = (uint64_t)(uintptr_t)ep0_ring->trbs;
        phys_addr = vmm_virt_to_phys(virt_addr);
        if (phys_addr == 0) {
            extern uint64_t pmm_hhdm_offset;
            if (pmm_hhdm_offset && virt_addr >= pmm_hhdm_offset) {
                phys_addr = virt_addr - pmm_hhdm_offset;
            } else {
                klog_printf(KLOG_ERROR, "xhci_device: failed to get physical address for EP0 ring");
                kfree(ep0_ring->trbs);
                kfree(ep0_ring);
                xhci_input_context_free(input_ctx);
                return -1;
            }
        }
        ep0_ring->phys_addr = (phys_addr_t)phys_addr;
        
        /* Setup Endpoint 0 Context using helper function */
        uint16_t max_packet_size = (speed == XHCI_SPEED_SUPER) ? 512 : 64;
        extern void xhci_input_context_set_ep0(xhci_input_context_t *ctx, struct xhci_transfer_ring *transfer_ring,
                                               uint16_t max_packet_size);
        xhci_input_context_set_ep0(input_ctx, (struct xhci_transfer_ring *)ep0_ring, max_packet_size);
    }
    
    /* Copy Slot Context to Output Context (Device Context) */
    /* Output Context is what gets stored in DCBAAP[slot_id] */
    /* Copy Slot Context (32 bytes) */
    for (int i = 0; i < 8; i++) {
        ((uint32_t *)&output_ctx->slot)[i] = ((uint32_t *)&input_ctx->slot)[i];
    }
    
    /* Copy EP0 Context to Output Context only if it was set up */
    if (include_ep0) {
        for (int i = 0; i < 8; i++) {
            ((uint32_t *)&output_ctx->endpoints[0])[i] = ((uint32_t *)&input_ctx->endpoints[0])[i];
        }
    }
    
    /* Get physical address of Output Context */
    uint64_t output_virt_addr = (uint64_t)(uintptr_t)output_ctx;
    uint64_t output_phys_addr = vmm_virt_to_phys(output_virt_addr);
    if (output_phys_addr == 0) {
        extern uint64_t pmm_hhdm_offset;
        if (pmm_hhdm_offset && output_virt_addr >= pmm_hhdm_offset) {
            output_phys_addr = output_virt_addr - pmm_hhdm_offset;
        } else {
            klog_printf(KLOG_ERROR, "xhci_device: failed to get physical address for Output Context");
            kfree(ep0_ring->trbs);
            kfree(ep0_ring);
            xhci_input_context_free(input_ctx);
            kfree(output_ctx_alloc);
            return -1;
        }
    }
    
    /* Allocate structure to store contexts */
    xhci_device_contexts_t *ctxs = (xhci_device_contexts_t *)kmalloc(sizeof(xhci_device_contexts_t));
    if (!ctxs) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to allocate contexts structure");
        kfree(ep0_ring->trbs);
        kfree(ep0_ring);
        xhci_input_context_free(input_ctx);
        kfree(output_ctx_alloc);
        return -1;
    }
    ctxs->input_ctx = input_ctx;
    ctxs->output_ctx = output_ctx;
    ctxs->output_ctx_phys = output_phys_addr;
    ctxs->ep0_ring = ep0_ring;
    
    /* Flush cache for Input Context, Output Context, and EP0 ring (if allocated) */
    extern void xhci_flush_cache(void *addr, size_t sz);
    xhci_flush_cache(input_ctx, sizeof(xhci_input_context_t));
    xhci_flush_cache(output_ctx, output_ctx_size);
    if (ep0_ring && ep0_ring->trbs) {
        size_t trb_array_size = ep0_ring->size * 16;
        xhci_flush_cache(ep0_ring->trbs, trb_array_size);
    }
    
    /* Store contexts structure in device driver_data */
    dev->driver_data = ctxs;
    
    /* Store EP0 ring temporarily if it was allocated - we'll move it to xhci->transfer_rings[slot_id][0] after Enable Slot */
    if (ep0_ring) {
        if (!xhci->transfer_rings[0][0]) {
            xhci->transfer_rings[0][0] = ep0_ring;
        } else {
            klog_printf(KLOG_WARN, "xhci_device: transfer_rings[0][0] already in use, storing EP0 ring elsewhere");
        }
    }
    
    if (include_ep0) {
        uint16_t max_packet_size = (speed == XHCI_SPEED_SUPER) ? 512 : 64;
        klog_printf(KLOG_INFO, "xhci_device: Input Context prepared (with EP0): speed=%u max_packet=%u ep0_ring_phys=0x%016llx output_ctx_phys=0x%016llx",
                    speed, max_packet_size, (unsigned long long)phys_addr, (unsigned long long)output_phys_addr);
    } else {
        klog_printf(KLOG_INFO, "xhci_device: Input Context prepared (Slot Context only): speed=%u output_ctx_phys=0x%016llx",
                    speed, (unsigned long long)output_phys_addr);
    }
    
    return 0;
}

/**
 * Enable Slot command
 * For USB 3.0, Input Context must be prepared BEFORE this call
 * Returns slot ID on success, 0 on failure
 */
uint32_t xhci_enable_slot(xhci_controller_t *xhci, usb_device_t *dev, uint8_t speed) {
    if (!xhci) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid controller");
        return 0;
    }
    
    /* For USB 3.0 (SuperSpeed), prepare Input Context BEFORE Enable Slot
     * CRITICAL: For Enable Slot, Input Context should contain ONLY Slot Context (no EP0 Context)
     * EP0 Context will be added in Address Device command.
     */
    if (speed == XHCI_SPEED_SUPER || speed == 5) { /* SuperSpeed or SuperSpeedPlus */
        if (xhci_prepare_input_context(xhci, dev, speed, false) < 0) { /* include_ep0 = false for Enable Slot */
            klog_printf(KLOG_ERROR, "xhci_device: failed to prepare Input Context for USB 3.0");
            return 0;
        }
    }
    
    klog_printf(KLOG_INFO, "xhci_device: enabling slot...");
    
    /* CRITICAL: Verify controller is in RUN state before sending command
     * Check USBSTS.HCH (Host Controller Halted) - must be 0
     * Check USBSTS.CNR (Controller Not Ready) - must be 0
     * Check USBCMD.RS (Run/Stop) - must be 1
     */
    uint32_t usbsts = xhci->op_regs->USBSTS;
    uint32_t usbcmd = xhci->op_regs->USBCMD;
    
    if (usbsts & XHCI_STS_HCH) {
        klog_printf(KLOG_ERROR, "xhci_device: controller is HALTED (USBSTS.HCH=1)! Cannot send command.");
        return 0;
    }
    
    if (usbsts & XHCI_STS_CNR) {
        klog_printf(KLOG_ERROR, "xhci_device: controller is NOT READY (USBSTS.CNR=1)! Cannot send command.");
        return 0;
    }
    
    if (!(usbcmd & XHCI_CMD_RUN)) {
        klog_printf(KLOG_ERROR, "xhci_device: controller is NOT RUNNING (USBCMD.RS=0)! Cannot send command.");
        return 0;
    }
    
    if (usbsts & XHCI_STS_HSE) {
        klog_printf(KLOG_ERROR, "xhci_device: Host System Error detected (USBSTS.HSE=1)! Cannot send command.");
        return 0;
    }
    
    klog_printf(KLOG_DEBUG, "xhci_device: controller state verified: RUN=1 HCH=0 CNR=0 HSE=0");
    
    /* Build Enable Slot TRB
     * Note: cycle bit will be set by xhci_cmd_ring_enqueue() to match ring cycle_state
     */
    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    trb.parameter = 0;
    trb.status = 0;
    trb.control = (XHCI_TRB_TYPE_ENABLE_SLOT << XHCI_TRB_TYPE_SHIFT);
    /* Cycle bit will be set by xhci_cmd_ring_enqueue() */
    
    if (xhci_cmd_ring_enqueue(&xhci->cmd_ring, &trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to enqueue Enable Slot TRB");
        return 0;
    }
    
    /* Ring command doorbell */
    xhci_ring_cmd_doorbell(xhci);
    __asm__ volatile("mfence" ::: "memory");
    
    /* Wait for completion event */
    xhci_event_trb_t event;
    if (xhci_wait_for_command_completion(xhci, &event, 1000000) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: Enable Slot command timed out");
        return 0;
    }
    
    uint8_t completion_code = event.status & XHCI_EVENT_TRB_COMPLETION_CODE_MASK;
    uint8_t slot_id = (event.control >> XHCI_EVENT_TRB_SLOT_ID_SHIFT) & XHCI_EVENT_TRB_SLOT_ID_MASK;
    
    if (completion_code != XHCI_TRB_CC_SUCCESS) {
        klog_printf(KLOG_ERROR, "xhci_device: Enable Slot failed code=%u", completion_code);
        return 0;
    }
    if (slot_id == 0 || slot_id > xhci->num_slots) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid slot ID %u", slot_id);
        return 0;
    }
    
    klog_printf(KLOG_INFO, "xhci_device: slot %u enabled", slot_id);
    
    /* For USB 3.0, move EP0 ring from temporary location to proper slot */
    if ((speed == XHCI_SPEED_SUPER || speed == 5) && xhci->transfer_rings[0][0]) {
        xhci_transfer_ring_t *ep0_ring = xhci->transfer_rings[0][0];
        xhci->transfer_rings[0][0] = NULL; /* Clear temporary */
        xhci->transfer_rings[slot_id][0] = ep0_ring; /* Move to proper slot */
        klog_printf(KLOG_INFO, "xhci_device: EP0 ring moved to slot %u", slot_id);
    }
    
    /* CRITICAL: For USB 3.0, write Output Context (Device Context) physical address to DCBAAP[slot_id]
     * This must be done AFTER Enable Slot completes successfully
     * DCBAAP[slot_id] must point to a 64-byte aligned Device Context
     */
    if ((speed == XHCI_SPEED_SUPER || speed == 5) && dev && dev->driver_data) {
        xhci_device_contexts_t *ctxs = (xhci_device_contexts_t *)dev->driver_data;
        if (ctxs && ctxs->output_ctx_phys != 0) {
            /* Verify Output Context is 64-byte aligned */
            if ((ctxs->output_ctx_phys & 0x3F) != 0) {
                klog_printf(KLOG_ERROR, "xhci_device: Output Context not 64-byte aligned! phys=0x%016llx",
                            (unsigned long long)ctxs->output_ctx_phys);
                return 0;
            }
            
            /* Write Output Context (Device Context) physical address to DCBAAP[slot_id] */
            xhci->dcbaap[slot_id] = ctxs->output_ctx_phys;
            
            /* Flush cache for DCBAAP entry BEFORE controller reads it */
            extern void xhci_flush_cache(void *addr, size_t sz);
            xhci_flush_cache(&xhci->dcbaap[slot_id], sizeof(uint64_t));
            
            /* Memory barrier to ensure write is visible to controller */
            __asm__ volatile("mfence" ::: "memory");
            
            /* Verify DCBAAP was written correctly */
            uint64_t dcbaap_read = xhci->dcbaap[slot_id];
            if (dcbaap_read != ctxs->output_ctx_phys) {
                klog_printf(KLOG_ERROR, "xhci_device: DCBAAP[%u] write verification failed! wrote=0x%016llx read=0x%016llx",
                            slot_id, (unsigned long long)ctxs->output_ctx_phys, (unsigned long long)dcbaap_read);
            } else {
                klog_printf(KLOG_INFO, "xhci_device: DCBAAP[%u] = 0x%016llx (Output Context/Device Context) verified",
                            slot_id, (unsigned long long)ctxs->output_ctx_phys);
            }
        } else {
            klog_printf(KLOG_ERROR, "xhci_device: Output Context not prepared for USB 3.0 device");
        }
    }
    
    return slot_id;
}

/**
 * Address Device command
 * Sets up device context with Input Context
 */
int xhci_address_device(xhci_controller_t *xhci, uint32_t slot_id, xhci_input_context_t *input_ctx,
                        uint64_t input_ctx_phys) {
    if (!xhci || slot_id == 0 || slot_id > xhci->num_slots || !input_ctx) {
        klog_printf(KLOG_ERROR, "xhci_device: invalid parameters for Address Device");
        return -1;
    }
    
    klog_printf(KLOG_INFO, "xhci_device: addressing device slot=%u input_ctx_phys=0x%016llx",
                slot_id, (unsigned long long)input_ctx_phys);
    
    /* Initialize transfer ring for endpoint 0 if not already done */
    uint32_t ep_idx = 0;
    if (!xhci->transfer_rings[slot_id][ep_idx]) {
        if (xhci_transfer_ring_init(xhci, slot_id, ep_idx) < 0) {
            klog_printf(KLOG_ERROR, "xhci_device: failed to init transfer ring for EP0");
            return -1;
        }
    }
    
    /* Build Address Device TRB */
    xhci_trb_t trb;
    k_memset(&trb, 0, sizeof(xhci_trb_t));
    
    trb.parameter = input_ctx_phys; /* Physical address of Input Context */
    trb.status = (slot_id & 0xFF) | (XHCI_TRB_IOC << 16); /* Slot ID in lower 8 bits, IOC */
    trb.control = (XHCI_TRB_TYPE_ADDRESS_DEVICE << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE;
    
    /* Enqueue to command ring */
    if (xhci_cmd_ring_enqueue(&xhci->cmd_ring, &trb) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: failed to enqueue Address Device TRB");
        return -1;
    }
    
    /* Ring command doorbell */
    xhci_ring_cmd_doorbell(xhci);
    
    /* Wait for completion */
    xhci_event_trb_t event;
    if (xhci_wait_for_command_completion(xhci, &event, 1000000) < 0) {
        klog_printf(KLOG_ERROR, "xhci_device: Address Device command failed");
        return -1;
    }
    
    /* Set DCBAA[slot_id] to Output Context (we'll allocate it later if needed) */
    /* For now, we just mark slot as allocated */
    xhci->slot_allocated[slot_id - 1] = 1;
    
    klog_printf(KLOG_INFO, "xhci_device: device addressed successfully (slot=%u)", slot_id);
    return 0;
}

