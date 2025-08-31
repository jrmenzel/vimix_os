/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

typedef uint32_t CPU_Features;
#define RV_SV32_SUPPORTED 0x01
#define RV_SV39_SUPPORTED 0x02
#define RV_SV48_SUPPORTED 0x04
#define RV_SV57_SUPPORTED 0x08
#define RV_EXT_FLOAT 0x10
#define RV_EXT_DOUBLE 0x20
#define RV_EXT_SSTC 0x40

/// read and write tp, the thread pointer, which VIMIX uses to hold
/// this core's hartid (core number), the index into g_cpus[].
/// This is required as the Hart ID can only be read in Machine Mode, but
/// the OS needs to know the ID in Supervisor Mode.
static inline size_t __arch_smp_processor_id()
{
    size_t x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

/// @brief which interrupts are reported IF interrupts are globaly enabled
static inline void cpu_set_interrupt_mask()
{
    // enable external, timer, software interrupts
    rv_write_csr_sie(rv_read_csr_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
}

/// enable device interrupts
static inline void cpu_enable_interrupts()
{
    rv_write_csr_sstatus(rv_read_csr_sstatus() | SSTATUS_SIE);
}

/// disable device interrupts
static inline void cpu_disable_interrupts()
{
    rv_write_csr_sstatus(rv_read_csr_sstatus() & ~SSTATUS_SIE);
}

/// are device interrupts enabled?
static inline bool cpu_is_interrupts_enabled()
{
    xlen_t x = rv_read_csr_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

/// Set the Supervisor-mode trap vector (interrupt handler) function
static inline void cpu_set_trap_vector(void *supervisor_trap_vector)
{
    rv_write_csr_stvec((xlen_t)supervisor_trap_vector);
}

/// @brief let the CPU sleep until the next interrupt occurs
static inline void wait_for_interrupt() { asm volatile("wfi"); }
