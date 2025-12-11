/*
 * AstraOS Source-Available License (ASAL v2.1)
 * Copyright (c) 2025 Krystian "v3nn7"
 * All rights reserved.
 *
 * Viewing allowed.
 * Modification, forking, redistribution, and commercial use prohibited
 * without explicit written permission from the author.
 *
 * Full license: see LICENSE.md or https://github.com/v3nn7
 */

// Stub implementation for ACPI _OSC USB control request.
#include "ACPI_OSC_USB.hpp"
#include "acpi.h"
#include "klog.h"

namespace acpi {

static bool g_usb_osc_done = false;
static bool g_usb_osc_granted = false;

bool request_usb_osc() {
    if (g_usb_osc_done) return g_usb_osc_granted;

#ifdef HOST_TEST
    g_usb_osc_granted = true;
#else
    acpi_init();
    klog_printf(KLOG_INFO, "acpi: _OSC USB not implemented, assuming firmware ownership");
    g_usb_osc_granted = true;
#endif

    g_usb_osc_done = true;
    return g_usb_osc_granted;
}

}  // namespace acpi
