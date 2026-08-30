/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/param.h>

#define ARCH_NAME_STRING "ARM 64"

static inline void arch_init_system()
{
    // nothing to do: the PSCI firmware will get discovered through the DTB
    // and gets initialized with all the other devices.
}

syserr_t system_boot_cpu(size_t cpu_idx, const void *dtb, size_t start_pa);

bool system_did_cpu_start(size_t cpu_idx);
