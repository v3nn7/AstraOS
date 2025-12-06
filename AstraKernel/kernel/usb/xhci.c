#include "xhci.h"
#include "kernel.h"
#include "drivers.h"
#include "interrupts.h"

#define XHCI_CAPLENGTH      0x00
#define XHCI_HCSPARAMS1     0x04
#define XHCI_RTSOFF         0x18

static volatile uint32_t *mmio;

static inline uint32_t mmio_read32(uint32_t off){
    return mmio[off/4];
}
static inline void mmio_write32(uint32_t off, uint32_t val){
    mmio[off/4] = val;
}

void xhci_irq_handler(void){
    uint32_t sts = mmio_read32(0x60); // USBSTS
    mmio_write32(0x60, sts);          // acknowledge
}

static void start_controller(){
    uint32_t USBCMD = 0x20;
    uint32_t USBSTS = 0x60;

    uint32_t cmd = mmio_read32(USBCMD);
    cmd |= 1; // RUN/STOP
    mmio_write32(USBCMD, cmd);

    while (mmio_read32(USBSTS) & (1<<0))
        ;
}

void xhci_init(usb_controller_t *ctrl){
    printf("XHCI: init at %lx\n", ctrl->mmio_base);

    mmio = (volatile uint32_t*)ctrl->mmio_base;

    int cap = mmio_read32(XHCI_CAPLENGTH) & 0xFF;
    printf("XHCI: CAPLENGTH=%d\n", cap);

    start_controller();

    printf("XHCI: running\n");
}

int xhci_init_one(uint8_t bus, uint8_t slot, uint8_t func, uint16_t vendor, uint16_t device) {
    (void)vendor; (void)device;
    /* Minimal stub: controller discovery to be extended with PCI MMIO mapping */
    usb_controller_t ctrl = {
        .bus = bus,
        .slot = slot,
        .func = func,
        .type = 3, /* XHCI */
        .mmio_base = 0
    };
    xhci_init(&ctrl);
    return 0;
}
