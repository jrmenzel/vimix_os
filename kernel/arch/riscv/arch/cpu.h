/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include "riscv.h"

/// read and write tp, the thread pointer, which xv6 uses to hold
/// this core's hartid (core number), the index into cpus[].
static inline uint64 r_tp()
{
    uint64 x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

/// enable device interrupts
static inline void intr_on() { w_sstatus(r_sstatus() | SSTATUS_SIE); }

/// disable device interrupts
static inline void intr_off() { w_sstatus(r_sstatus() & ~SSTATUS_SIE); }

/// are device interrupts enabled?
static inline int intr_get()
{
    uint64 x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}
