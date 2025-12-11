#include "../include/xhci.h"

#include <stddef.h>
#include <stdint.h>

extern void* memset(void* dest, int val, size_t n);

static void make_completion_event(xhci_trb_t* evt, uint64_t ptr, uint32_t code) {
    evt->parameter_low = (uint32_t)(ptr & 0xFFFFFFFFu);
    evt->parameter_high = (uint32_t)(ptr >> 32);
    evt->status = code;
    evt->control = (XHCI_TRB_EVENT_DATA << 10) | 1u;  // set cycle bit
}

static bool post_completion(xhci_controller_t* ctrl, uint64_t ptr, uint32_t code) {
    xhci_trb_t evt;
    memset(&evt, 0, sizeof(evt));
    make_completion_event(&evt, ptr, code);
    return xhci_event_push(&ctrl->evt_ring, &evt);
}

bool xhci_cmd_enable_slot(xhci_controller_t* ctrl, uint8_t* slot_out) {
    if (!ctrl || !slot_out) {
        return false;
    }
    xhci_trb_t* cmd = xhci_ring_enqueue(&ctrl->cmd_ring, XHCI_TRB_ENABLE_SLOT);
    if (!cmd) {
        return false;
    }
    *slot_out = 1;
    return post_completion(ctrl, 0, 1);
}

bool xhci_cmd_address_device(xhci_controller_t* ctrl, uint8_t slot_id, uint64_t input_ctx_phys) {
    if (!ctrl || slot_id == 0) {
        return false;
    }
    xhci_trb_t* cmd = xhci_ring_enqueue(&ctrl->cmd_ring, XHCI_TRB_ADDRESS_DEVICE);
    if (!cmd) {
        return false;
    }
    cmd->parameter_low = (uint32_t)(input_ctx_phys & 0xFFFFFFFFu);
    cmd->parameter_high = (uint32_t)(input_ctx_phys >> 32);
    return post_completion(ctrl, input_ctx_phys, 1);
}

bool xhci_cmd_configure_endpoint(xhci_controller_t* ctrl, uint8_t slot_id, uint64_t input_ctx_phys) {
    if (!ctrl || slot_id == 0) {
        return false;
    }
    xhci_trb_t* cmd = xhci_ring_enqueue(&ctrl->cmd_ring, XHCI_TRB_CONFIGURE_ENDPOINT);
    if (!cmd) {
        return false;
    }
    cmd->parameter_low = (uint32_t)(input_ctx_phys & 0xFFFFFFFFu);
    cmd->parameter_high = (uint32_t)(input_ctx_phys >> 32);
    return post_completion(ctrl, input_ctx_phys, 1);
}
