/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/trap.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <syscalls/syscall.h>

/// return how many clock tick interrupts have occurred
/// since start.
uint64_t sys_uptime()
{
    uint32_t xticks;

    spin_lock(&g_tickslock);
    xticks = g_ticks;
    spin_unlock(&g_tickslock);
    return xticks;
}
