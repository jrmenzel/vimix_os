/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include "riscv.h"

/// read and write tp, the thread pointer, which VIMIX uses to hold
/// this core's hartid (core number), the index into g_cpus[].
static inline uint64 __arch_smp_processor_id()
{
    uint64 x;
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
static inline int cpu_is_device_interrupts_enabled()
{
    uint64 x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}
