/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/reset.h>
#include <arch/trap.h>
#include <drivers/rtc.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/reboot.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>
#include <syscalls/syscall.h>

size_t sys_uptime() { return kticks_get_ticks(); }

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

ssize_t get_time_to_user(size_t tloc_va)
{
    time_t time = rtc_get_time();
    struct process *proc = get_current();

    int32_t res =
        uvm_copy_out(proc->pagetable, tloc_va, (char *)&time, sizeof(time_t));
    return res;
}

size_t sys_get_time()
{
    // parameter 0: time_t *tloc
    size_t tloc_va;
    argaddr(0, &tloc_va);
    return get_time_to_user(tloc_va);
}