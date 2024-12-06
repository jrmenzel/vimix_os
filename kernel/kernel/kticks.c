/* SPDX-License-Identifier: MIT */

#ifdef __SBI_CONSOLE__
#include <arch/riscv/sbi.h>
#endif

#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <timer.h>

// each call to the timer interrupt is one tick
struct spinlock g_tickslock;
size_t g_ticks = 0;

/// @brief boot time from rv_get_time()
uint64_t g_boot_time;

void kticks_init()
{
    g_ticks = 0;
    spin_lock_init(&g_tickslock, "time");
}

void kticks_inc_ticks()
{
    spin_lock(&g_tickslock);
    g_ticks++;
    wakeup(&g_ticks);
    spin_unlock(&g_tickslock);

#ifdef __SBI_CONSOLE__
    sbi_console_poll_input();
#endif
}

size_t kticks_get_ticks()
{
    spin_lock(&g_tickslock);
    size_t xticks = g_ticks;
    spin_unlock(&g_tickslock);
    return xticks;
}

size_t seconds_since_boot()
{
    uint64_t now = rv_get_time();
    uint64_t delta = now - g_boot_time;
    return delta / g_timebase_frequency;
}
