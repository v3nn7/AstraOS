// xHCI controller scaffolding: hardware-aware init plus fallbacks.
#include "xhci.hpp"

#include <stddef.h>
#include <stdint.h>

#include "usb_keyboard.hpp"

extern "C" void* memset(void* dest, int val, size_t n);
extern "C" uint32_t mmio_read32(uintptr_t addr);
extern "C" void mmio_write32(uintptr_t addr, uint32_t val);
extern "C" uint8_t mmio_read8(uintptr_t addr);
extern "C" bool pci_find_xhci(uintptr_t* mmio_base_out, uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out);

namespace usb {
uint32_t mmio_read32(uintptr_t addr) { return ::mmio_read32(addr); }
uint8_t mmio_read8(uintptr_t addr) { return ::mmio_read8(addr); }
void mmio_write32(uintptr_t addr, uint32_t val) { ::mmio_write32(addr, val); }
bool pci_find_xhci(uintptr_t* base, uint8_t* b, uint8_t* d, uint8_t* f) {
    return ::pci_find_xhci(base, b, d, f);
}
}  // namespace usb

namespace usb {

namespace {

constexpr size_t kCmdRingSize = 32;
constexpr size_t kEventRingSize = 64;
constexpr size_t kIntrRingSize = 32;
constexpr size_t kEp0RingSize = 32;
constexpr size_t kEpIntrRingSize = 32;

// Bits
constexpr uint32_t kUsbcmdRun = 1 << 0;
constexpr uint32_t kUsbcmdReset = 1 << 1;
constexpr uint32_t kUsbcmdInth = 1 << 2;
constexpr uint32_t kUsbstsHchalted = 1 << 0;
constexpr uint32_t kUsbstsCnr = 1 << 11;

constexpr uint32_t kPortPr = 1 << 4;
constexpr uint32_t kPortCcs = 1 << 0;
constexpr uint32_t kPortPp = 1 << 9;

constexpr uint32_t kImanIe = 1 << 1;

constexpr uint32_t kTrbCycle = 1;

// Small hex helpers for logging without libc.
void u32_to_hex(uint32_t v, char* out) {
    for (int i = 0; i < 8; ++i) {
        const uint8_t nibble = (v >> ((7 - i) * 4)) & 0xF;
        out[i] = (nibble < 10) ? static_cast<char>('0' + nibble) : static_cast<char>('A' + (nibble - 10));
    }
    out[8] = 0;
}

[[maybe_unused]] void u64_to_hex(uint64_t v, char* out) {
    for (int i = 0; i < 16; ++i) {
        const uint8_t nibble = (v >> ((15 - i) * 4)) & 0xF;
        out[i] = (nibble < 10) ? static_cast<char>('0' + nibble) : static_cast<char>('A' + (nibble - 10));
    }
    out[16] = 0;
}

}  // namespace

XhciController::XhciController() = default;

XhciController::~XhciController() {
    if (cmd_ring_.ring) kfree(cmd_ring_.ring);
    if (event_ring_.ring) kfree(event_ring_.ring);
    if (intr_ring_.ring) kfree(intr_ring_.ring);
    if (ep0_ring_.ring) kfree(ep0_ring_.ring);
    if (intr_ep_ring_.ring) kfree(intr_ep_ring_.ring);
    if (dcbaa_) kfree(dcbaa_);
    if (scratchpad_) kfree(scratchpad_);
    if (slot_ctx_) kfree(slot_ctx_);
    if (input_ctx_) kfree(input_ctx_);
}

void XhciController::log(const char* msg) { klog(msg); }

void XhciController::init_rings() {
    auto init_ring = [](TrbRing& r, size_t n) {
        r.size = n;
        r.ring = reinterpret_cast<Trb*>(kmemalign(64, sizeof(Trb) * r.size));
        r.enqueue = r.dequeue = 0;
        r.cycle = 1;
        r.phys = virt_to_phys(r.ring);
    };
    init_ring(cmd_ring_, kCmdRingSize);
    init_ring(event_ring_, kEventRingSize);
    init_ring(intr_ring_, kIntrRingSize);
    init_ring(ep0_ring_, kEp0RingSize);
    init_ring(intr_ep_ring_, kEpIntrRingSize);
}

Trb* XhciController::push_cmd_trb() {
    if (!cmd_ring_.ring) return nullptr;
    Trb& trb = cmd_ring_.ring[cmd_ring_.enqueue];
    cmd_ring_.enqueue = (cmd_ring_.enqueue + 1) % cmd_ring_.size;
    // Ring command doorbell (DB0)
#ifndef HOST_TEST
    uintptr_t db0 = mmio_ + doorbell_offset_;
    mmio_write32(db0, 0);
#endif
    return &trb;
}

Trb* XhciController::push_intr_trb() {
    if (!intr_ring_.ring) return nullptr;
    Trb& trb = intr_ring_.ring[intr_ring_.enqueue];
    intr_ring_.enqueue = (intr_ring_.enqueue + 1) % intr_ring_.size;
    return &trb;
}

bool XhciController::init() {
    init_rings();
    log("xhci: rings initialized");
    if (!locate_controller()) {
        log("xhci: no controller found, running in stub mode");
        return true;
    }
    if (!init_op_regs()) {
        log("xhci: op regs init failed");
        return false;
    }
    if (!init_command_ring()) {
        log("xhci: command ring init failed");
        return false;
    }
    if (!init_event_ring()) {
        log("xhci: event ring init failed");
        return false;
    }
    if (!set_dcbaa()) {
        log("xhci: dcbaa setup failed");
        return false;
    }
    select_port();
    if (!start_controller()) {
        log("xhci: start failed");
        return false;
    }
    hw_ready_ = true;
    log("xhci: hardware path ready");
    return true;
}

bool XhciController::reset_first_port() {
    if (!hw_ready_) {
        log("xhci: resetting port (stub)");
        return true;
    }
    const uintptr_t portsc = mmio_ + portsc_offset_;
    uint32_t before = mmio_read32(portsc);
    char buf[40];
    u32_to_hex(before, buf);
    log("xhci: PORTSC before reset");
    log(buf);
    // Ensure port power on.
    mmio_write32(portsc, before | kPortPp);
    uint32_t powered = mmio_read32(portsc);
    u32_to_hex(powered, buf);
    log("xhci: PORTSC after power");
    log(buf);
    mmio_write32(portsc, powered | kPortPr);
    bool ok = wait_for_port_ready(portsc_offset_);
    uint32_t after = mmio_read32(portsc);
    u32_to_hex(after, buf);
    log("xhci: PORTSC after reset");
    log(buf);
    if (!ok) {
        log("xhci: port reset timeout");
    }
    return ok;
}

bool XhciController::wait_for_port_ready(uint32_t portsc_addr) {
    for (int i = 0; i < 1000000; ++i) {
        uint32_t v = mmio_read32(mmio_ + portsc_addr);
        if ((v & kPortPr) == 0 && (v & kPortCcs)) {
            return true;
        }
    }
    return false;
}

void XhciController::select_port() {
#ifndef HOST_TEST
    // Scan ports to find one with CCS set; fallback to first.
    if (port_count_ == 0) {
        port_count_ = 1;
    }
    for (uint32_t i = 0; i < port_count_; ++i) {
        uintptr_t off = portsc_base_ + i * 0x10;
        uint32_t v = mmio_read32(mmio_ + off);
        char buf[40];
        u32_to_hex(off, buf);
        log("xhci: port scan offset");
        log(buf);
        u32_to_hex(v, buf);
        log("xhci: port scan value");
        log(buf);
        if (v & kPortCcs) {
            portsc_offset_ = off;
            u32_to_hex(portsc_offset_, buf);
            log("xhci: selected port offset");
            log(buf);
            return;
        }
    }
    // Fallback port 1
    portsc_offset_ = portsc_base_;
#endif
}

bool XhciController::locate_controller() {
#ifndef HOST_TEST
    uintptr_t base = 0;
    uint8_t b = 0, d = 0, f = 0;
    if (!pci_find_xhci(&base, &b, &d, &f)) {
        return false;
    }
    mmio_ = base;
    char buf[40];
    u64_to_hex(static_cast<uint64_t>(base), buf);
    log("xhci: MMIO base");
    log(buf);
    // Capability-derived offsets.
    cap_length_ = mmio_read8(mmio_ + 0x00);
    hcs_params1_ = mmio_read32(mmio_ + 0x04);
    hcc_params1_ = mmio_read32(mmio_ + 0x10);
    // RTSoFF is at offset 0x18; bits 31:5 hold 32-byte units.
    uint32_t rtsoff = mmio_read32(mmio_ + 0x18) & 0xFFFFFFE0u;
    doorbell_offset_ = mmio_read32(mmio_ + 0x14) & 0xFFFFFFFCu;
    op_offset_ = static_cast<uint32_t>(cap_length_);
    runtime_offset_ = rtsoff;
    port_count_ = (hcs_params1_ >> 24) & 0xFF;
    portsc_base_ = op_offset_ + 0x400;  // first port
    portsc_offset_ = portsc_base_;
    u32_to_hex(op_offset_, buf);
    log("xhci: OP offset");
    log(buf);
    u32_to_hex(runtime_offset_, buf);
    log("xhci: RT offset");
    log(buf);
    u32_to_hex(portsc_offset_, buf);
    log("xhci: PORTSC offset");
    log(buf);
    u32_to_hex(doorbell_offset_, buf);
    log("xhci: DB offset");
    log(buf);
    u32_to_hex(port_count_, buf);
    log("xhci: port count (HCS1)");
    log(buf);
    return true;
#else
    (void)cap_length_;
    (void)hcs_params1_;
    (void)hcc_params1_;
    (void)op_offset_;
    (void)runtime_offset_;
    (void)portsc_offset_;
    log("xhci: resetting port 1");
    return false;
#endif
}

uint8_t XhciController::enable_slot() {
    Trb* trb = push_cmd_trb();
    if (!trb) return 0;
    trb->data[0] = 0;
    trb->data[1] = 0;
    trb->data[2] = 0;
    trb->data[3] = (static_cast<uint32_t>(TrbType::EnableSlot) << 10) | kTrbCycle;
    log("xhci: enable slot cmd queued");
    // Hardware completion would set slot, stub assumes 1.
    return 1;
}

bool XhciController::address_device(UsbDevice* dev) {
    if (!dev) return false;
    Trb* trb = push_cmd_trb();
    if (!trb) return false;
    uintptr_t ictx = input_ctx_phys_;
    trb->data[0] = static_cast<uint32_t>(ictx & 0xFFFFFFFFu);
    trb->data[1] = static_cast<uint32_t>((ictx >> 32) & 0xFFFFFFFFu);
    trb->data[2] = 0;
    trb->data[3] = (static_cast<uint32_t>(TrbType::AddressDevice) << 10) | kTrbCycle | (dev->slot_id() << 24);
    log("xhci: address device cmd queued");
    return true;
}

bool XhciController::configure_endpoints_for_keyboard(UsbDevice* dev) {
    if (!dev) return false;
    Trb* trb = push_cmd_trb();
    if (!trb) return false;
    uintptr_t ictx = input_ctx_phys_;
    trb->data[0] = static_cast<uint32_t>(ictx & 0xFFFFFFFFu);
    trb->data[1] = static_cast<uint32_t>((ictx >> 32) & 0xFFFFFFFFu);
    trb->data[2] = 0;
    trb->data[3] = (static_cast<uint32_t>(TrbType::ConfigureEndpoint) << 10) | kTrbCycle | (dev->slot_id() << 24);
    log("xhci: configure endpoint cmd queued");
    return true;
}

bool XhciController::submit_interrupt_in(UsbDevice* /*dev*/, uint8_t /*endpoint*/, void* buffer, size_t len) {
    Trb* trb = push_intr_trb();
    if (!trb) return false;
    uintptr_t addr = reinterpret_cast<uintptr_t>(buffer);
    trb->data[0] = static_cast<uint32_t>(addr & 0xFFFFFFFFu);
    trb->data[1] = static_cast<uint32_t>((addr >> 32) & 0xFFFFFFFFu);
    trb->data[2] = static_cast<uint32_t>(len);
    trb->data[3] = (static_cast<uint32_t>(TrbType::Normal) << 10) | kTrbCycle;
    last_intr_buffer_ = buffer;
    last_intr_len_ = len;
    return true;
}

void XhciController::handle_event(const Trb& trb) {
    const uint8_t type = static_cast<uint8_t>((trb.data[3] >> 10) & 0x3F);
    switch (static_cast<TrbType>(type)) {
        case TrbType::TransferEvent:
            // Normally we would match the transfer and hand data to the driver.
            if (keyboard_driver_ && last_intr_buffer_ && last_intr_len_ == 8) {
                keyboard_driver_->handle_report(reinterpret_cast<uint8_t*>(last_intr_buffer_));
            }
            break;
        case TrbType::PortStatusChange:
            log("xhci: port status change");
            break;
        case TrbType::CommandCompletion:
            log("xhci: command completion");
            break;
        default:
            log("xhci: unhandled event");
            break;
    }
}

void XhciController::poll() {
    // In real hardware, this would read event ring dequeue pointer.
    if (!hw_ready_) {
        if (!event_ring_.ring) return;
        while (event_ring_.dequeue != event_ring_.enqueue) {
            Trb& trb = event_ring_.ring[event_ring_.dequeue];
            event_ring_.dequeue = (event_ring_.dequeue + 1) % event_ring_.size;
            handle_event(trb);
        }
        return;
    }
#ifndef HOST_TEST
    // Hardware path: read ERDP and process until cycle bit mismatch.
    size_t safety = kEventRingSize;
    while (safety--) {
        Trb& ev = event_ring_.ring[event_ring_.dequeue];
        const uint8_t cycle = ev.data[3] & 1;
        if (cycle != event_ring_.cycle) {
            break;
        }
        event_ring_.dequeue = (event_ring_.dequeue + 1) % event_ring_.size;
        if (event_ring_.dequeue == 0) {
            event_ring_.cycle ^= 1;
        }
        handle_event(ev);
        uint64_t erdp = event_ring_.phys + static_cast<uint64_t>(event_ring_.dequeue * sizeof(Trb));
        uintptr_t runtime = mmio_ + runtime_offset_;
        mmio_write32(runtime + 0x18, static_cast<uint32_t>(erdp & 0xFFFFFFFFu));
        mmio_write32(runtime + 0x1C, static_cast<uint32_t>((erdp >> 32) & 0xFFFFFFFFu));
    }
#endif
}

bool XhciController::init_op_regs() {
#ifndef HOST_TEST
    // Halt controller
    mmio_write32(mmio_ + op_offset_ + 0x00, 0);
    for (int i = 0; i < 1000000; ++i) {
        if (mmio_read32(mmio_ + op_offset_ + 0x04) & kUsbstsHchalted) {
            break;
        }
    }
    // Reset controller
    mmio_write32(mmio_ + op_offset_ + 0x00, kUsbcmdReset);
    for (int i = 0; i < 1000000; ++i) {
        if ((mmio_read32(mmio_ + op_offset_ + 0x00) & kUsbcmdReset) == 0) {
            break;
        }
    }
    // Clear status
    mmio_write32(mmio_ + op_offset_ + 0x04, 0xFFFFFFFF);
    // Enable interrupts
    uintptr_t iman = mmio_ + runtime_offset_ + 0x20;
    uintptr_t imod = mmio_ + runtime_offset_ + 0x24;
    mmio_write32(iman, kImanIe);
    mmio_write32(imod, 0);
    uint32_t sts = mmio_read32(mmio_ + op_offset_ + 0x04);
    uint32_t cmd = mmio_read32(mmio_ + op_offset_ + 0x00);
    char buf[40];
    u32_to_hex(sts, buf);
    log("xhci: USBSTS after reset");
    log(buf);
    u32_to_hex(cmd, buf);
    log("xhci: USBCMD after reset");
    log(buf);
    return true;
#else
    return true;
#endif
}

bool XhciController::init_command_ring() {
    if (!cmd_ring_.ring) return false;
#ifndef HOST_TEST
    uintptr_t crcr = cmd_ring_.phys;
    mmio_write32(mmio_ + op_offset_ + 0x18, static_cast<uint32_t>(crcr & 0xFFFFFFFFu) | (cmd_ring_.cycle & 1));
    mmio_write32(mmio_ + op_offset_ + 0x1C, static_cast<uint32_t>((crcr >> 32) & 0xFFFFFFFFu));
#endif
    return true;
}

bool XhciController::init_event_ring() {
    if (!event_ring_.ring) return false;
#ifndef HOST_TEST
    // Simple one-segment ERST with one entry.
    struct ErstEntry {
        uint64_t addr;
        uint32_t size;
        uint32_t rsvd;
    } __attribute__((packed));
    ErstEntry* erst = reinterpret_cast<ErstEntry*>(kmemalign(64, sizeof(ErstEntry)));
    if (!erst) return false;
    erst->addr = event_ring_.phys;
    erst->size = static_cast<uint32_t>(event_ring_.size);
    erst->rsvd = 0;

    uintptr_t erstba = virt_to_phys(erst);
    uintptr_t intr0 = mmio_ + runtime_offset_ + 0x20;  // interrupter 0 base
    mmio_write32(intr0 + 0x08, 1);  // ERSTSZ
    mmio_write32(intr0 + 0x10, static_cast<uint32_t>(erstba & 0xFFFFFFFFu));
    mmio_write32(intr0 + 0x14, static_cast<uint32_t>((erstba >> 32) & 0xFFFFFFFFu));
    uintptr_t erdp = event_ring_.phys;
    mmio_write32(intr0 + 0x18, static_cast<uint32_t>(erdp & 0xFFFFFFFFu));
    mmio_write32(intr0 + 0x1C, static_cast<uint32_t>((erdp >> 32) & 0xFFFFFFFFu));
#endif
    return true;
}

bool XhciController::start_controller() {
#ifndef HOST_TEST
    // Enable interrupts in runtime already set; now run and INTE.
    // Ensure CNR is clear before run.
    for (int i = 0; i < 1000000; ++i) {
        if ((mmio_read32(mmio_ + op_offset_ + 0x04) & kUsbstsCnr) == 0) {
            break;
        }
    }
    uint32_t cmd = mmio_read32(mmio_ + op_offset_ + 0x00);
    cmd |= kUsbcmdRun | kUsbcmdInth;
    mmio_write32(mmio_ + op_offset_ + 0x00, cmd);
    // Wait until not halted and not CNR.
    for (int i = 0; i < 1000000; ++i) {
        uint32_t sts = mmio_read32(mmio_ + op_offset_ + 0x04);
        if ((sts & kUsbstsHchalted) == 0 && (sts & kUsbstsCnr) == 0) {
            return true;
        }
    }
    log("xhci: start timeout");
    return false;
#else
    return true;
#endif
}

bool XhciController::set_dcbaa() {
#ifndef HOST_TEST
    // Read HCSParams1 for scratchpad count.
    uint32_t hcs1 = mmio_read32(mmio_ + op_offset_ + 0x04);
    uint32_t max_slots = hcs1 & 0xFF;
    uint32_t spb_lo = (hcs1 >> 27) & 0x1F;
    uint32_t spb_hi = (hcs1 >> 21) & 0x1F;
    uint32_t scratchpads = (spb_hi << 5) | spb_lo;
    if (max_slots < 1) {
        return false;
    }
    dcbaa_ = reinterpret_cast<uint64_t*>(kmemalign(64, sizeof(uint64_t) * (max_slots + 1)));
    if (!dcbaa_) return false;
    memset(dcbaa_, 0, sizeof(uint64_t) * (max_slots + 1));
    if (scratchpads > 0) {
        scratchpad_ = kmemalign(4096, 4096 * scratchpads);
        if (!scratchpad_) return false;
        dcbaa_[0] = virt_to_phys(scratchpad_);
    }
    // Allocate contexts for slot 1.
    slot_ctx_ = reinterpret_cast<DeviceContext*>(kmemalign(64, sizeof(DeviceContext)));
    input_ctx_ = reinterpret_cast<InputContext*>(kmemalign(64, sizeof(InputContext)));
    if (!slot_ctx_ || !input_ctx_) {
        return false;
    }
    memset(slot_ctx_, 0, sizeof(DeviceContext));
    memset(input_ctx_, 0, sizeof(InputContext));
    slot_ctx_phys_ = virt_to_phys(slot_ctx_);
    input_ctx_phys_ = virt_to_phys(input_ctx_);
    dcbaa_[1] = slot_ctx_phys_;

    uintptr_t dcbaa_ptr = virt_to_phys(dcbaa_);
    mmio_write32(mmio_ + op_offset_ + 0x30, static_cast<uint32_t>(dcbaa_ptr & 0xFFFFFFFFu));
    mmio_write32(mmio_ + op_offset_ + 0x34, static_cast<uint32_t>((dcbaa_ptr >> 32) & 0xFFFFFFFFu));
    // Allow 1 slot.
    mmio_write32(mmio_ + op_offset_ + 0x38, 1);
#endif
    (void)slot_ctx_phys_;
    (void)input_ctx_phys_;
    return true;
}

}  // namespace usb
