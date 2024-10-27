/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/reset.h>
#include <arch/trap.h>
#include <drivers/rtc.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/mount.h>
#include <kernel/reboot.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>
#include <syscalls/syscall.h>

ssize_t sys_uptime() { return kticks_get_ticks(); }

ssize_t sys_reboot()
{
    // parameter 0: cmd
    int32_t cmd;
    argint(0, &cmd);

    // validate input:
    if (cmd != VIMIX_REBOOT_CMD_POWER_OFF && cmd != VIMIX_REBOOT_CMD_RESTART)
    {
        return -EINVAL;
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
    return -EOTHER;
}

ssize_t get_time_to_user(size_t tloc_va)
{
    time_t time = rtc_get_time();
    struct process *proc = get_current();

    int32_t res =
        uvm_copy_out(proc->pagetable, tloc_va, (char *)&time, sizeof(time_t));
    return (res < 0) ? -ENOMEM : res;
}

ssize_t sys_get_time()
{
    // parameter 0: time_t *tloc
    size_t tloc_va;
    argaddr(0, &tloc_va);
    return get_time_to_user(tloc_va);
}

ssize_t sys_mount()
{
    size_t idx = 0;
    // parameter 0: const char *source
    char source[PATH_MAX];
    if (argstr(idx++, source, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: const char *target
    char target[PATH_MAX];
    if (argstr(idx++, target, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 2: const char *filesystemtype
    char filesystemtype[64];
    if (argstr(idx++, filesystemtype, 64) < 0)
    {
        return -EFAULT;
    }

    // parameter 3: unsigned long mountflags
    unsigned long mountflags;
    argaddr(idx++, &mountflags);

    // parameter 4: const void *data
    size_t addr_data;
    argaddr(idx++, &addr_data);

    return mount(source, target, filesystemtype, mountflags, addr_data);
}

ssize_t sys_umount()
{
    // parameter 0: const char *target
    char target[PATH_MAX];
    if (argstr(0, target, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return umount(target);
}
