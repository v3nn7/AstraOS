/**
 * USB request helpers.
 *
 * Defines the setup packet used by control transfers and a small helper
 * to initialise it.
 */

#include <drivers/usb/usb_core.h>

typedef struct PACKED usb_setup_packet {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

static inline void usb_request_build(usb_setup_packet_t *pkt, uint8_t bmRequestType,
                                     uint8_t bRequest, uint16_t wValue,
                                     uint16_t wIndex, uint16_t wLength) {
    if (!pkt) {
        return;
    }
    pkt->bmRequestType = bmRequestType;
    pkt->bRequest = bRequest;
    pkt->wValue = wValue;
    pkt->wIndex = wIndex;
    pkt->wLength = wLength;
}
