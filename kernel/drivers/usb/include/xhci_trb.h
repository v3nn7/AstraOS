#ifndef XHCI_TRB_H
#define XHCI_TRB_H

#include <stdint.h>

/* Basic TRB layout used by command/event rings. */
typedef struct xhci_trb {
    uint32_t parameter_low;
    uint32_t parameter_high;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

typedef enum {
    XHCI_TRB_NORMAL = 1,
    XHCI_TRB_SETUP_STAGE = 2,
    XHCI_TRB_DATA_STAGE = 3,
    XHCI_TRB_STATUS_STAGE = 4,
    XHCI_TRB_LINK = 6,
    XHCI_TRB_EVENT_DATA = 7,
    XHCI_TRB_ENABLE_SLOT = 9,
    XHCI_TRB_ADDRESS_DEVICE = 11,
    XHCI_TRB_CONFIGURE_ENDPOINT = 12,
    XHCI_TRB_PORT_STATUS_CHANGE = 34
} xhci_trb_type_t;

#endif /* XHCI_TRB_H */
