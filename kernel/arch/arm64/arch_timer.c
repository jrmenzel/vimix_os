/* SPDX-License-Identifier: MIT */

#include <arch/arm64/arm64.h>
#include <kernel/timer.h>

static void enable_timer()
{
    size_t c = arm_read_cntv_ctl_el0();
    c |= CNTV_CTL_ENABLE;
    c &= ~CNTV_CTL_IMASK;
    arm_write_cntv_ctl_el0(c);
}

static void disable_timer()
{
    size_t c = arm_read_cntv_ctl_el0();
    c &= ~CNTV_CTL_ENABLE;
    c |= CNTV_CTL_IMASK;
    arm_write_cntv_ctl_el0(c);
}

static void reload_timer(uint64_t interval)
{
    arm_write_cntv_tval_el0(interval);
}

void arm_schedule_interrupt(uint64_t time, uint64_t interval)
{
    disable_timer();
    reload_timer(interval);
    enable_timer();
}

timer_schedule_interrupt_p *arch_timer_interrupt_func()
{
    return arm_schedule_interrupt;
}
