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

#pragma once

// ACPI _OSC helper for hub/root-complex features.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace acpi {

// Request OS control of hub/root-complex features via ACPI _OSC.
// Returns true when control is granted or when the platform lacks _OSC support.
bool request_hub_osc();

}  // namespace acpi

