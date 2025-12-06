#include "types.h"
#include "drivers.h"
#include "kernel.h"
#include "interrupts.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t pci_cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read_vendor(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint16_t)(pci_cfg_read(bus, slot, func, 0x00) & 0xFFFF);
}

void pci_scan(void) {
    printf("PCI: scanning...\n");
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor = pci_read_vendor((uint8_t)bus, slot, 0);
            if (vendor == 0xFFFF) continue;
            uint32_t id = pci_cfg_read((uint8_t)bus, slot, 0, 0x00);
            uint32_t classreg = pci_cfg_read((uint8_t)bus, slot, 0, 0x08);
            uint16_t device = (uint16_t)(id >> 16);
            uint8_t class = (uint8_t)(classreg >> 24);
            uint8_t subclass = (uint8_t)(classreg >> 16);
            uint8_t progif = (uint8_t)(classreg >> 8);
            printf("PCI: %02x:%02x.0 ven=%04x dev=%04x class=%02x sub=%02x prog=%02x\n",
                   (uint8_t)bus, slot, vendor, device, class, subclass, progif);
        }
    }
}
