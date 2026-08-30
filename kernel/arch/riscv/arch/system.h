/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <sbi.h>

#define ARCH_NAME_STRING "RISC V"

static inline void arch_init_system() { init_sbi(); }

static inline syserr_t system_boot_cpu(size_t cpu_idx, const void *dtb,
                                       size_t start_pa)
{
    return sbi_start_hart(cpu_idx, (size_t)dtb, start_pa);
}

static inline bool system_did_cpu_start(size_t cpu_idx)
{
    return sbi_did_hart_start(cpu_idx);
}
