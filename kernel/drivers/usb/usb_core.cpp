// Core USB orchestration and descriptor parsing.
#include "usb_core.hpp"

#include <stddef.h>

#include "xhci.hpp"
#include "usb_hid.hpp"
#include "usb_keyboard.hpp"

// Provide placement new without depending on <new>.
inline void* operator new(size_t, void* p) noexcept { return p; }

namespace usb {

namespace {

void log(const char* msg) { klog(msg); }

}  // namespace

UsbDevice::UsbDevice() = default;

bool UsbDevice::parse_descriptors(const uint8_t* data, size_t length) {
    // Minimal parser: expects device + configuration + interface + endpoint.
    size_t offset = 0;
    bool have_device = false;
    bool have_config = false;
    UsbInterface* current_interface = nullptr;
    while (offset + sizeof(UsbDescriptorHeader) <= length) {
        const auto* hdr = reinterpret_cast<const UsbDescriptorHeader*>(data + offset);
        if (hdr->length == 0 || offset + hdr->length > length) {
            break;
        }
        switch (hdr->descriptor_type) {
            case 1:  // Device
                if (hdr->length <= sizeof(UsbDeviceDescriptor)) {
                    const auto* dev = reinterpret_cast<const UsbDeviceDescriptor*>(hdr);
                    device_desc_ = *dev;
                    have_device = true;
                }
                break;
            case 2:  // Configuration
                if (hdr->length <= sizeof(UsbConfigurationDescriptor)) {
                    const auto* cfg = reinterpret_cast<const UsbConfigurationDescriptor*>(hdr);
                    configuration_.value = cfg->configuration_value;
                    configuration_.interface_count = 0;
                    have_config = true;
                }
                break;
            case 4:  // Interface
                if (!have_config || configuration_.interface_count >= 4) {
                    break;
                }
                if (hdr->length <= sizeof(UsbInterfaceDescriptor)) {
                    const auto* ifc = reinterpret_cast<const UsbInterfaceDescriptor*>(hdr);
                    UsbInterface& slot = configuration_.interfaces[configuration_.interface_count++];
                    slot.number = ifc->interface_number;
                    slot.alternate_setting = ifc->alternate_setting;
                    slot.class_code = ifc->interface_class;
                    slot.subclass = ifc->interface_subclass;
                    slot.protocol = ifc->interface_protocol;
                    slot.endpoint_count = 0;
                    current_interface = &slot;
                }
                break;
            case 5:  // Endpoint
                if (current_interface == nullptr || current_interface->endpoint_count >= 4) {
                    break;
                }
                if (hdr->length <= sizeof(UsbEndpointDescriptor)) {
                    const auto* ep = reinterpret_cast<const UsbEndpointDescriptor*>(hdr);
                    UsbEndpoint& out = current_interface->endpoints[current_interface->endpoint_count++];
                    out.address = ep->endpoint_address;
                    out.attributes = ep->attributes;
                    out.max_packet_size = ep->max_packet_size;
                    out.interval = ep->interval;
                    const uint8_t type = ep->attributes & 0x3;
                    switch (type) {
                        case 0: out.type = UsbTransferType::Control; break;
                        case 1: out.type = UsbTransferType::Isochronous; break;
                        case 2: out.type = UsbTransferType::Bulk; break;
                        case 3: out.type = UsbTransferType::Interrupt; break;
                        default: out.type = UsbTransferType::Interrupt; break;
                    }
                }
                break;
            default:
                break;
        }
        offset += hdr->length;
    }
    return have_device && have_config;
}

bool UsbDevice::set_configuration(uint8_t value) {
    if (configuration_.value == value) {
        configured_ = true;
        return true;
    }
    return false;
}

UsbConfiguration* UsbDevice::configuration() { return &configuration_; }

UsbCore::UsbCore() = default;

UsbCore::~UsbCore() {
    if (keyboard_driver_) {
        kfree(keyboard_driver_);
    }
    if (keyboard_device_) {
        kfree(keyboard_device_);
    }
    if (xhci_) {
        kfree(xhci_);
    }
}

bool UsbCore::init() {
    xhci_ = reinterpret_cast<XhciController*>(kmalloc(sizeof(XhciController)));
    keyboard_device_ = reinterpret_cast<UsbDevice*>(kmalloc(sizeof(UsbDevice)));
    keyboard_driver_ = reinterpret_cast<UsbKeyboardDriver*>(kmalloc(sizeof(UsbKeyboardDriver)));
    if (!xhci_ || !keyboard_device_ || !keyboard_driver_) {
        log("usb: allocation failed");
        return false;
    }
    new (xhci_) XhciController();
    new (keyboard_device_) UsbDevice();
    new (keyboard_driver_) UsbKeyboardDriver();

    if (!xhci_->init()) {
        log("usb: xhci init failed");
        return false;
    }
    xhci_->set_keyboard_driver(keyboard_driver_);
    return probe_keyboard();
}

bool UsbCore::probe_keyboard() {
    // In a real stack we would enumerate ports, read descriptors, and so on.
    // Here we rely on the controller helper to perform the minimal flow.
    if (!xhci_->reset_first_port()) {
        log("usb: port reset failed");
        return false;
    }
    const uint8_t slot = xhci_->enable_slot();
    if (slot == 0) {
        log("usb: enable slot failed");
        return false;
    }
    keyboard_device_->set_slot_id(slot);
    keyboard_device_->set_speed(UsbSpeed::Full);
    if (!xhci_->address_device(keyboard_device_)) {
        log("usb: address device failed");
        return false;
    }
    if (!xhci_->configure_endpoints_for_keyboard(keyboard_device_)) {
        log("usb: configure endpoints failed");
        return false;
    }
    keyboard_device_->set_configuration(1);
    keyboard_driver_->bind(xhci_, keyboard_device_);
    return true;
}

void UsbCore::poll() {
    if (xhci_) {
        xhci_->poll();
    }
}

static UsbCore* g_core = nullptr;

void usb_init() {
    g_core = reinterpret_cast<UsbCore*>(kmalloc(sizeof(UsbCore)));
    if (!g_core) {
        klog("usb: core alloc failed");
        return;
    }
    new (g_core) UsbCore();
    if (!g_core->init()) {
        klog("usb: init failed");
    } else {
        klog("usb: initialized");
    }
}

void usb_poll() {
    if (g_core) {
        g_core->poll();
    }
}

}  // namespace usb
