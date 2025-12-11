// Stub for ACPI _OSC USB handoff.
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace acpi {

// Request OS control of USB (XHCI/USB-C) via _OSC.
// Returns true if granted, false otherwise (stubbed on non-ACPI builds).
bool request_usb_osc();

}  // namespace acpi
