// xHCI controller scaffolding for AstraOS (hardware-aware with fallbacks).
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "usb_core.hpp"

namespace usb {

// Low-level MMIO helpers provided by platform code.
uint32_t mmio_read32(uintptr_t addr);
void mmio_write32(uintptr_t addr, uint32_t val);
uint8_t mmio_read8(uintptr_t addr);

// Platform hook: locate xHCI and return MMIO base (physical) plus BDF if needed.
bool pci_find_xhci(uintptr_t* mmio_base_out, uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out);

// Doorbells, registers, and TRB types are kept minimal for clarity.
enum class TrbType : uint8_t {
    Normal = 1,
    SetupStage = 2,
    DataStage = 3,
    StatusStage = 4,
    Isoch = 5,
    Link = 6,
    EventData = 7,
    NoOp = 8,
    EnableSlot = 9,
    AddressDevice = 11,
    ConfigureEndpoint = 12,
    TransferEvent = 32,
    CommandCompletion = 33,
    PortStatusChange = 34,
};

struct __attribute__((packed, aligned(16))) Trb {
    uint32_t data[4];
};

struct TrbRing {
    Trb* ring;
    size_t size;
    size_t enqueue;
    size_t dequeue;
    uint8_t cycle;
    uint64_t phys;
};

struct __attribute__((packed, aligned(32))) SlotContext {
    uint32_t route_speed;     // route string + speed
    uint32_t ctxt_entries;    // context entries
    uint32_t tt_info;
    uint32_t state;
    uint32_t reserved[4];
};

struct __attribute__((packed, aligned(32))) EndpointContext {
    uint32_t ep_state;
    uint32_t mult_maxpstreams;
    uint32_t tr_deq_low;
    uint32_t tr_deq_high;
    uint32_t tx_info;
    uint32_t max_pkt_intr;
    uint32_t reserved[2];
};

struct __attribute__((packed, aligned(32))) DeviceContext {
    SlotContext slot;
    EndpointContext eps[31];
};

struct __attribute__((packed, aligned(32))) InputControlContext {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[6];
};

struct __attribute__((packed, aligned(64))) InputContext {
    InputControlContext control;
    SlotContext slot;
    EndpointContext eps[31];
};

class XhciController {
public:
    XhciController();
    ~XhciController();

    bool init();
    bool reset_first_port();
    uint8_t enable_slot();
    bool address_device(UsbDevice* dev);
    bool configure_endpoints_for_keyboard(UsbDevice* dev);

    void poll();

    void set_keyboard_driver(UsbKeyboardDriver* drv) { keyboard_driver_ = drv; }

    // Interrupt IN submission for keyboard reports.
    bool submit_interrupt_in(UsbDevice* dev, uint8_t endpoint, void* buffer, size_t len);

private:
    // Hardware state
    uintptr_t mmio_{0};
    bool hw_ready_{false};
    uint32_t cap_length_{0};
    uint32_t hcs_params1_{0};
    uint32_t hcc_params1_{0};
    uint32_t op_offset_{0};
    uint32_t runtime_offset_{0};
    uint32_t portsc_base_{0};
    uint32_t portsc_offset_{0};
    uint32_t port_count_{0};
    uint32_t doorbell_offset_{0};

    // Context arrays
    uint64_t* dcbaa_{nullptr};
    void* scratchpad_{nullptr};
    DeviceContext* slot_ctx_{nullptr};
    InputContext* input_ctx_{nullptr};
    uint64_t slot_ctx_phys_{0};
    uint64_t input_ctx_phys_{0};

    TrbRing cmd_ring_{};
    TrbRing event_ring_{};
    TrbRing intr_ring_{};
    TrbRing ep0_ring_{};
    TrbRing intr_ep_ring_{};

    UsbKeyboardDriver* keyboard_driver_{nullptr};
    void* last_intr_buffer_{nullptr};
    size_t last_intr_len_{0};

    void init_rings();
    Trb* push_cmd_trb();
    Trb* push_intr_trb();
    void handle_event(const Trb& trb);
    void log(const char* msg);

    // Hardware helpers
    bool locate_controller();
    bool init_op_regs();
    bool init_event_ring();
    bool init_command_ring();
    bool set_dcbaa();
    bool start_controller();
    void select_port();
    bool wait_for_port_ready(uint32_t portsc_addr);
};

}  // namespace usb
