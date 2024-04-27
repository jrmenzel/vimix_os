/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/trap.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <syscalls/syscall.h>

size_t sys_uptime()
{
    spin_lock(&g_tickslock);
    size_t xticks = g_ticks;
    spin_unlock(&g_tickslock);
    return xticks;
}
