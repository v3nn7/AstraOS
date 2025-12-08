#pragma once

#include "types.h"
#include <stdint.h>

/* Supported Protocol Capability */
typedef struct PACKED {
    uint32_t capability_id:8;
    uint32_t next_ptr:8;
    uint32_t minor_rev:8;
    uint32_t major_rev:8;
    uint32_t name_string;
    uint32_t compatible_port_offset:8;
    uint32_t compatible_port_count:8;
    uint32_t protocol_defined:12;
    uint32_t psic:4;
    uint32_t protocol_slot_type:4;
    uint32_t reserved:28;
} xhci_protocol_cap_t;

