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

// Minimal implementation for ACPI _OSC hub/root-complex control request.
#include "ACPI_OSC_HUB.hpp"
#include "acpi.h"
#include "klog.h"

namespace acpi {

static bool g_hub_osc_done = false;
static bool g_hub_osc_granted = false;

bool request_hub_osc() {
    if (g_hub_osc_done) {
        return g_hub_osc_granted;
    }

#ifdef HOST_TEST
    // Host tests do not evaluate firmware AML; assume success.
    g_hub_osc_granted = true;
#else
    // ACPI interpreter not yet present; ensure ACPI is initialized and log once.
    acpi_init();
    klog_printf(KLOG_INFO, "acpi: _OSC hub/root not implemented, assuming firmware ownership");
    g_hub_osc_granted = true;
#endif

    g_hub_osc_done = true;
    return g_hub_osc_granted;
}

}  // namespace acpi

