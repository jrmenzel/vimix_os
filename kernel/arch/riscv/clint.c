/* SPDX-License-Identifier: MIT */
#include "clint.h"

#include <arch/cpu.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

/// a scratch area per CPU for machine-mode timer interrupts.
uint64_t g_m_mode_interrupt_handler_scratchpads[MAX_CPUS][5];

/// assembly code in m_mode_trap_vector.S for machine-mode timer interrupt.
extern void m_mode_trap_vector();

/// arrange to receive timer interrupts.
/// they will arrive in machine mode at
/// at m_mode_trap_vector in m_mode_trap_vector.S,
/// which turns them into software interrupts for
/// interrupt_handler() in trap.c.
void clint_init_timer_interrupt()
{
    // each CPU has a separate source of timer interrupts.
    int id = r_mhartid();

    // ask the CLINT for a timer interrupt.
    int interval = 1000000;  // cycles; about 1/10th second in qemu.
    *(uint64_t *)CLINT_MTIMECMP(id) = *(uint64_t *)CLINT_MTIME + interval;

    // prepare information in this_harts_scratch[] for m_mode_trap_vector.
    // this_harts_scratch[0..2] : space for m_mode_trap_vector to save
    // registers. this_harts_scratch[3] : address of CLINT MTIMECMP register.
    // this_harts_scratch[4] : desired interval (in cycles) between timer
    // interrupts.
    uint64_t *this_harts_scratch =
        &g_m_mode_interrupt_handler_scratchpads[id][0];
    this_harts_scratch[3] = CLINT_MTIMECMP(id);
    this_harts_scratch[4] = interval;
    w_mscratch((uint64_t)this_harts_scratch);

    // set the machine-mode trap handler.
    w_mtvec((uint64_t)m_mode_trap_vector);

    // enable machine-mode interrupts.
    w_mstatus(r_mstatus() | MSTATUS_MIE);

    // enable machine-mode timer interrupts.
    w_mie(r_mie() | MIE_MTIE);
}
