/* SPDX-License-Identifier: MIT */
/*
 * Based on xv6.
 */

//
// System Information system calls.
//

#include <arch/trap.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <syscalls/syscall.h>

/// return how many clock tick interrupts have occurred
/// since start.
uint64 sys_uptime(void)
{
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}
