/* SPDX-License-Identifier: MIT */
#include "clint.h"

#include <arch/cpu.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

#ifndef __ENABLE_SBI__

/// a scratch area per CPU for machine-mode timer interrupts.
size_t m_mode_interrupt_handler_scratchpads[MAX_CPUS][6];

/// assembly code in m_mode_trap_vector.S:
extern void m_mode_trap_vector();

void clint_init_timer_interrupt()
{
    // each CPU has a separate source of timer interrupts.
    xlen_t id = rv_read_csr_mhartid();

    // ask the CLINT for a timer interrupt: also 64 bit on 32bit CPU!
    size_t timer_interrupt_interval =
        timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;

    // 64bit even on 32bit systems!
    *(uint64_t *)CLINT_MTIMECMP(id) =
        *(uint64_t *)CLINT_MTIME + timer_interrupt_interval;

    // prepare information in scratch[] for m_mode_trap_vector.
    // (addresses in elements)
    // this_harts_scratch[0..3] : space for m_mode_trap_vector to save
    // registers. this_harts_scratch[4] : address of CLINT MTIMECMP register.
    // this_harts_scratch[5] : desired interval (in cycles) between timer
    // interrupts.
    size_t *this_harts_scratch = &m_mode_interrupt_handler_scratchpads[id][0];
    this_harts_scratch[4] = CLINT_MTIMECMP(id);
    this_harts_scratch[5] = timer_interrupt_interval;
    rv_write_csr_mscratch((xlen_t)this_harts_scratch);

    // set interrupt handler and enable interrupts
    cpu_set_m_mode_trap_vector(m_mode_trap_vector);
    cpu_enable_m_mode_interrupts();
    cpu_enable_m_mode_timer_interrupt();
}

#endif  // __ENABLE_SBI__

#if (__riscv_xlen == 32)
uint64_t rv_get_time()
{
    uint32_t time_low = rv_read_csr_time();
    uint32_t time_high = rv_read_csr_timeh();

    return ((uint64_t)time_high << 32) + (uint64_t)time_low;
}

#else

uint64_t rv_get_time() { return rv_read_csr_time(); }
#endif
