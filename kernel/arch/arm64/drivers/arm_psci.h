/* SPDX-License-Identifier: MIT */
#pragma once

//
// On ARM64, the optional PSCI (Power State Coordination Interface) is used to
// call firmware functions, e.g. to power off the machine or start additional
// CPUs.
//
// It gets detected via the device tree and thus inits like a normal driver.
// Only the 64-bit calling convention is supported.
//

#include <kernel/kernel.h>

struct Device_Init_Parameters;

dev_t arm_psci_init(struct Device_Init_Parameters *init_parameters,
                    const char *name);

bool arm_psci_available();

int64_t arm_psci_cpu_on(uint64_t target_cpu, uint64_t entry_point,
                        uint64_t context_id);
