/* SPDX-License-Identifier: MIT */

#include <arch/timer.h>
#include <drivers/console.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>

// each call to the timer interrupt is one tick
struct spinlock g_tickslock;
size_t g_ticks = 0;

/// @brief boot time from rv_get_time()
uint64_t g_boot_time = 0;

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

    // The htif and SBI consoles can be a fallback for UART,
    // but without IRQs we need to poll the input manually
    if (g_console_poll_callback)
    {
        g_console_poll_callback();
    }
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
    uint64_t now = get_time();
    uint64_t delta = now - g_boot_time;
    return delta / g_timebase_frequency;
}
