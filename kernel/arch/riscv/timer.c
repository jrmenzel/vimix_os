/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <arch/riscv/sbi.h>
#include <arch/riscv/timer.h>

timer_schedule_interrupt_p *timer_schedule_interrupt = NULL;

//
// schedule interrupt functions of supported timers

#if defined(__TIMER_SOURCE_CLINT)
// happens in M-Mode, nothing to do in S-Mode
void clint_schedule_interrupt(uint64_t) {}
#endif

#if defined(__ENABLE_SBI__) && defined(__TIMER_SOURCE_SBI)
void sbi_schedule_interrupt(uint64_t next_timer_intr)
{
    sbi_set_timer(next_timer_intr);
}
#endif

#if defined(__TIMER_SOURCE_SSTC)
void sstc_schedule_interrupt(uint64_t next_timer_intr)
{
    rv_set_stimecmp(next_timer_intr);
}
#endif

//
// init one of the supported timers

void timer_init()
{
#if defined(__TIMER_SOURCE_CLINT)
    // ask for clock interrupts.
    clint_init_timer_interrupt();
    timer_schedule_interrupt = clint_schedule_interrupt;
#endif

#if defined(__ENABLE_SBI__) && defined(__TIMER_SOURCE_SBI)
    if (sbi_probe_extension(SBI_EXT_ID_TIME) > 0)
    {
        timer_schedule_interrupt = sbi_schedule_interrupt;
    }
#endif

#if defined(__TIMER_SOURCE_SSTC)
    timer_schedule_interrupt = sstc_schedule_interrupt;
#endif

    if (timer_schedule_interrupt == NULL)
    {
        panic("NO TIMER FOUND\n");
    }

    uint64_t timer_interrupt_interval =
        timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;
    uint64_t now = rv_get_time();
    timer_schedule_interrupt(now + timer_interrupt_interval);
}