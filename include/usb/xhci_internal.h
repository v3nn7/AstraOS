#pragma once

#include "usb/xhci.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t *buffer_array;
    phys_addr_t buffer_array_phys;
    void **buffers;
    phys_addr_t *buffer_phys;
    uint32_t num_buffers;
} xhci_scratchpad_t;

struct xhci_controller {
    /* Register mappings */
    xhci_cap_regs_t *cap_regs;
    xhci_op_regs_t *op_regs;
    xhci_rt_regs_t *rt_regs;
    xhci_doorbell_regs_t *doorbell_regs;
    xhci_port_regs_t *port_regs;
    xhci_intr_regset_t *intr0;
    
    /* Capability information */
    uint32_t cap_length;
    uint32_t hci_version;
    uint32_t num_slots;
    uint32_t num_ports;
    uint32_t max_interrupters;
    uint32_t page_size;
    uint32_t max_psa_size;
    
    /* Extended capabilities */
    void *ext_caps;
    uint32_t ext_caps_offset;
    
    /* Feature flags */
    bool has_64bit_addressing;
    bool has_bandwidth_negotiation;
    bool has_context_size_64;
    bool has_port_power_control;
    bool has_port_indicators;
    bool has_light_hc_reset;
    bool has_latency_tolerance;
    bool has_no_secondary_sid;
    bool has_parse_all_event_data;
    bool has_stopped_short_packet;
    bool has_stopped_edtla;
    bool has_contiguous_frame_id;
    bool has_scratchpad;
    
    /* Scratchpad buffers */
    xhci_scratchpad_t scratchpad;
    uint32_t scratchpad_size;
    void **scratchpad_buffers;
    
    /* Interrupt information */
    bool has_msi;
    bool has_msix;
    uint32_t irq;
    uint32_t msi_vector;
    
    /* Command and Event rings */
    xhci_command_ring_t cmd_ring;
    xhci_event_ring_t event_ring;
    
    /* Transfer rings - [slot_id][endpoint_id] */
    xhci_transfer_ring_t *transfer_rings[256][32];
    usb_transfer_t *active_control_transfers[256];
    
    /* Device contexts */
    xhci_device_context_t *device_contexts[256];
    xhci_input_context_t *input_contexts[256];
    
    /* Device Context Base Address Array Pointer */
    uint64_t *dcbaap;
    phys_addr_t dcbaap_phys;
    
    /* Slot allocation tracking */
    uint8_t slot_allocated[256];
    
    /* Port status tracking */
    uint32_t port_status[256];
    
    /* Protocol information per port */
    struct {
        uint8_t major_revision;
        uint8_t minor_revision;
        uint8_t protocol_slot_type;
        bool is_usb3;
    } port_protocol[256];
    
    /* Command completion tracking */
    struct {
        volatile bool completed;
        uint32_t completion_code;
        uint32_t slot_id;
        uint64_t command_trb_phys;
    } cmd_status;
    
    /* Transfer completion tracking */
    struct {
        volatile bool completed;
        uint32_t completion_code;
        uint32_t transferred_length;
    } transfer_status[256][32];
    
    /* Synchronization */
    void *lock;
    
    /* PCI device information */
    uint8_t pci_bus;
    uint8_t pci_device;
    uint8_t pci_function;
};

#ifdef __cplusplus
}
#endif

