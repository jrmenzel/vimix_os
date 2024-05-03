/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/trap.h>
#include <kernel/kernel.h>
#include <kernel/reboot.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

size_t sys_uptime()
{
    spin_lock(&g_tickslock);
    size_t xticks = g_ticks;
    spin_unlock(&g_tickslock);
    return xticks;
}

size_t sys_reboot()
{
    // parameter 0: cmd
    int32_t cmd;
    argint(0, &cmd);

    // validate input:
    if (cmd != VIMIX_REBOOT_CMD_POWER_OFF && cmd != VIMIX_REBOOT_CMD_RESTART)
    {
        return -1;
    }

    switch (cmd)
    {
        case VIMIX_REBOOT_CMD_POWER_OFF:
            (*(volatile uint32_t *)VIRT_TEST) = VIRT_TEST_SHUTDOWN;
            break;
        case VIMIX_REBOOT_CMD_RESTART:
            (*(volatile uint32_t *)VIRT_TEST) = VIRT_TEST_REBOOT;
        default: break;
    }

    panic("sys_reboot() failed\n");
    return -1;
}
