/* SPDX-License-Identifier: MIT */

#include <arch/timer.h>
#include <drivers/console.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>

// each call to the timer interrupt is one tick
atomic_size_t g_ticks = 0;

/// @brief boot time from rv_get_time()
uint64_t g_boot_time = 0;

void kticks_init() { atomic_init(&g_ticks, 0); }

void kticks_inc_ticks()
{
    atomic_fetch_add(&g_ticks, 1);

    // The htif and SBI consoles can be a fallback for UART,
    // but without IRQs we need to poll the input manually
    if (g_console_poll_callback)
    {
        g_console_poll_callback();
    }
}

size_t seconds_since_boot()
{
    uint64_t now = get_time();
    uint64_t delta = now - g_boot_time;
    return delta / g_timebase_frequency;
}
