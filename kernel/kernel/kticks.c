/* SPDX-License-Identifier: MIT */

#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>

#include <arch/riscv/sbi.h>

// each call to the timer interrupt is one tick
struct spinlock g_tickslock;
size_t g_ticks;

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
