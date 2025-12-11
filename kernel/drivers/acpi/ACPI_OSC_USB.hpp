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

// Stub for ACPI _OSC USB handoff.
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace acpi {

// Request OS control of USB (XHCI/USB-C) via _OSC.
// Returns true if granted, false otherwise (stubbed on non-ACPI builds).
bool request_usb_osc();

}  // namespace acpi
