/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include "riscv.h"

/// read and write tp, the thread pointer, which VIMIX uses to hold
/// this core's hartid (core number), the index into g_cpus[].
/// This is required as the Hart ID can only be read in Machine Mode, but
/// the OS needs to know the ID in Supervisor Mode.
static inline xlen_t __arch_smp_processor_id()
{
    xlen_t x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

/// enable device interrupts
static inline void cpu_enable_device_interrupts()
{
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

/// disable device interrupts
static inline void cpu_disable_device_interrupts()
{
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

/// are device interrupts enabled?
static inline bool cpu_is_device_interrupts_enabled()
{
    xlen_t x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}
