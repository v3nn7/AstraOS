// Core USB structures and plumbing for AstraOS.
#pragma once

#include <stdint.h>
#include <stddef.h>

// Kernel allocation and logging hooks provided elsewhere.
#ifdef __cplusplus
extern "C" {
#endif
void* kmalloc(size_t);
void  kfree(void*);
void  klog(const char*);
void  input_push_key(uint8_t key, bool pressed);
void* kmemalign(size_t alignment, size_t size);
uintptr_t virt_to_phys(const void* p);
#ifdef __cplusplus
}
#endif

namespace usb {

enum class UsbSpeed : uint8_t {
    Full = 1,
    High = 2,
    Super = 3,
};

enum class UsbTransferType : uint8_t {
    Control = 0,
    Isochronous = 1,
    Bulk = 2,
    Interrupt = 3,
};

struct __attribute__((packed)) UsbDescriptorHeader {
    uint8_t length;
    uint8_t descriptor_type;
};

struct __attribute__((packed)) UsbDeviceDescriptor {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t usb_bcd;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  max_packet_size0;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_bcd;
    uint8_t  manufacturer_index;
    uint8_t  product_index;
    uint8_t  serial_index;
    uint8_t  num_configurations;
};

struct __attribute__((packed)) UsbConfigurationDescriptor {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t total_length;
    uint8_t  num_interfaces;
    uint8_t  configuration_value;
    uint8_t  configuration_index;
    uint8_t  attributes;
    uint8_t  max_power;
};

struct __attribute__((packed)) UsbInterfaceDescriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_index;
};

struct __attribute__((packed)) UsbEndpointDescriptor {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint8_t  endpoint_address;
    uint8_t  attributes;
    uint16_t max_packet_size;
    uint8_t  interval;
};

struct UsbEndpoint {
    UsbTransferType type;
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
};

struct UsbInterface {
    uint8_t number;
    uint8_t alternate_setting;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t endpoint_count;
    UsbEndpoint endpoints[4];  // simple cap for HID
};

struct UsbConfiguration {
    uint8_t value;
    uint8_t interface_count;
    UsbInterface interfaces[4];  // HID keyboards have one interface
};

class XhciController;
class UsbKeyboardDriver;

class UsbDevice {
public:
    UsbDevice();

    bool parse_descriptors(const uint8_t* data, size_t length);
    bool set_configuration(uint8_t value);

    UsbConfiguration* configuration();
    const UsbDeviceDescriptor& device_descriptor() const { return device_desc_; }

    void set_slot_id(uint8_t id) { slot_id_ = id; }
    uint8_t slot_id() const { return slot_id_; }

    void set_address(uint8_t addr) { address_ = addr; }
    uint8_t address() const { return address_; }

    void set_speed(UsbSpeed spd) { speed_ = spd; }
    UsbSpeed speed() const { return speed_; }

private:
    UsbDeviceDescriptor device_desc_{};
    UsbConfiguration configuration_{};
    uint8_t slot_id_{0};
    uint8_t address_{0};
    UsbSpeed speed_{UsbSpeed::Full};
    bool configured_{false};
};

class UsbCore {
public:
    UsbCore();
    ~UsbCore();

    bool init();
    void poll();

private:
    XhciController* xhci_{nullptr};
    UsbDevice* keyboard_device_{nullptr};
    UsbKeyboardDriver* keyboard_driver_{nullptr};

    bool probe_keyboard();
};

// Convenience entry points for the kernel.
void usb_init();
void usb_poll();

}  // namespace usb
