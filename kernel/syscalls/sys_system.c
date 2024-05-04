/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/reset.h>
#include <arch/trap.h>
#include <kernel/kernel.h>
#include <kernel/reboot.h>
#include <kernel/spinlock.h>
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
            printk("Power off NOW!\n");
            machine_power_off();
            break;
        case VIMIX_REBOOT_CMD_RESTART:
            printk("Restart NOW!\n");
            machine_restart();
            break;
        default: break;
    }

    panic("sys_reboot() failed\n");
    return -1;
}
