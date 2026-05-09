/* SPDX-License-Identifier: MIT */

#include <kernel/timer.h>

timer_schedule_interrupt_p *timer_schedule_interrupt = NULL;

/// read from the device tree
uint64_t g_timebase_frequency = 0;

void timer_init(void *dtb, CPU_Features features)
{
    g_boot_time = get_time();

    timer_schedule_interrupt = arch_timer_interrupt_func(features);
    if (timer_schedule_interrupt == NULL)
    {
        panic("no timer source found");
    }

    uint64_t timer_interrupt_interval =
        g_timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;
    DEBUG_EXTRA_ASSERT(timer_interrupt_interval > 0, "invalid timebase");
    uint64_t now = get_time();
    timer_schedule_interrupt(now, timer_interrupt_interval);
}
