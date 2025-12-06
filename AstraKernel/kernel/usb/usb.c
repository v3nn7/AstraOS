#include "usb.h"
#include "drivers.h"
#include "kernel.h"
#include "interrupts.h"

#define MAX_USB_CTRLS 8
#define MAX_PCI_BUS   32  /* limit scan to first 32 buses to avoid HW hangs */
static usb_controller_t ctrls[MAX_USB_CTRLS];
static int ctrl_count = 0;

static bool usb_is_hypervisor(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x1), "c"(0));
    /* ECX bit 31 = hypervisor present */
    return (ecx & (1u << 31)) != 0;
}

static uint32_t pci_cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

void usb_init(void) {
    if (usb_is_hypervisor()) {
        printf("USB: hypervisor detected, skipping USB/HID\n");
        return;
    }
    ctrl_count = 0;
    printf("USB: scanning for EHCI/XHCI (buses 0-%d)...\n", MAX_PCI_BUS - 1);
    for (uint8_t bus = 0; bus < MAX_PCI_BUS; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor = (uint16_t)(pci_cfg_read(bus, slot, 0, 0x00) & 0xFFFF);
            if (vendor == 0xFFFF) continue;
            uint32_t classreg = pci_cfg_read(bus, slot, 0, 0x08);
            uint16_t device = (uint16_t)(pci_cfg_read(bus, slot, 0, 0x00) >> 16);
            uint8_t class = (uint8_t)(classreg >> 24);
            uint8_t subclass = (uint8_t)(classreg >> 16);
            uint8_t progif = (uint8_t)(classreg >> 8);
            if (class == 0x0C && subclass == 0x03) {
                usb_ctrl_type_t type = USB_CTRL_NONE;
                if (progif == 0x20) type = USB_CTRL_EHCI;
                else if (progif == 0x30) type = USB_CTRL_XHCI;
                if (type != USB_CTRL_NONE && ctrl_count < MAX_USB_CTRLS) {
                    ctrls[ctrl_count++] = (usb_controller_t){ type, bus, slot, 0, vendor, device };
                    printf("USB: found %s at %02x:%02x.0 ven=%04x dev=%04x\n",
                           (type == USB_CTRL_EHCI ? "EHCI" : "XHCI"), bus, slot, vendor, device);
                    if (type == USB_CTRL_XHCI) {
                        extern int xhci_init_one(uint8_t, uint8_t, uint8_t, uint16_t, uint16_t);
                        xhci_init_one(bus, slot, 0, vendor, device);
                    }
                }
            }
        }
    }
    if (ctrl_count == 0) {
        printf("USB: no controllers found, skipping HID\n");
        return;
    }
    printf("USB: controllers found=%d (XHCI/EHCI init stub; HID not active yet)\n", ctrl_count);
}

int usb_controller_count(void) {
    return ctrl_count;
}

