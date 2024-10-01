/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

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
    rv_write_csr_sstatus(rv_read_csr_sstatus() | SSTATUS_SIE);
}

/// disable device interrupts
static inline void cpu_disable_device_interrupts()
{
    rv_write_csr_sstatus(rv_read_csr_sstatus() & ~SSTATUS_SIE);
}

/// are device interrupts enabled?
static inline bool cpu_is_device_interrupts_enabled()
{
    xlen_t x = rv_read_csr_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

#if defined(__BOOT_M_MODE)
/// returns the Hart ID (~CPU/thread ID). CPU must be in Machine Mode to
/// execute.
static inline xlen_t cpu_read_hart_id_mhartid()
{
    return rv_read_csr_mhartid();
}

/// enable machine mode interrupts by setting bit MSTATUS_MIE in mstatus
static inline void cpu_enable_m_mode_interrupts()
{
    rv_write_csr_mstatus(rv_read_csr_mstatus() | MSTATUS_MIE);
}

/// enable machine mode timer interrupts
static inline void cpu_enable_m_mode_timer_interrupt()
{
    rv_write_csr_mie(rv_read_csr_mie() | MIE_MTIE);
}

/// Set the Machine-mode trap vector (interrupt handler) function
static inline void cpu_set_m_mode_trap_vector(void *machine_mode_trap_vector)
{
    rv_write_csr_mtvec((xlen_t)machine_mode_trap_vector);
}
#endif  // __BOOT_M_MODE

/// Set the Supervisor-mode trap vector (interrupt handler) function
static inline void cpu_set_s_mode_trap_vector(void *supervisor_mode_trap_vector)
{
    rv_write_csr_stvec((xlen_t)supervisor_mode_trap_vector);
}

/// @brief let the CPU sleep until the next interrupt occurs
static inline void wait_for_interrupt() { asm volatile("wfi"); }
