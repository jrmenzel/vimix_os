/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/trap.h>
#include <drivers/rtc.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/mount.h>
#include <kernel/proc.h>
#include <kernel/reboot.h>
#include <kernel/reset.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>
#include <syscalls/syscall.h>

ssize_t sys_uptime() { return kticks_get_ticks(); }

void system_shutdown()
{
    // stop other CPUs
    cpu_mask mask = ipi_cpu_mask_all_but_self();
    ipi_send_interrupt(mask, IPI_SHUTDOWN, NULL);

    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if (i == smp_processor_id()) continue;
        while (g_cpus[i].state != CPU_HALTED && g_cpus[i].state != CPU_UNUSED &&
               g_cpus[i].state != CPU_PANICKED)
        {
            // wait for other existing (!CPU_UNUSED) CPUs to halt or panic
            atomic_thread_fence(memory_order_seq_cst);
        }
    }

    printk("All other CPUs halted.\n");
}

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
            system_shutdown();
            machine_power_off();
            break;
        case VIMIX_REBOOT_CMD_RESTART:
            printk("Restart NOW!\n");
            system_shutdown();
            machine_restart();
            break;
        default: break;
    }

    panic("sys_reboot() failed\n");
    return -EOTHER;
}

ssize_t get_time_to_user(clockid_t clockid, size_t timespec_va)
{
    if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
    {
        return -EINVAL;
    }
    struct timespec time = rtc_get_time();
    struct process *proc = get_current();

    int32_t res = uvm_copy_out(proc->pagetable, timespec_va, (char *)&time,
                               sizeof(struct timespec));
    return (res < 0) ? -ENOMEM : 0;
}

ssize_t sys_clock_gettime()
{
    // parameter 0: cklockid
    clockid_t clock;
    argint(0, &clock);

    // parameter 1: timespec *tp
    size_t timespec_va;
    argaddr(1, &timespec_va);

    return get_time_to_user(clock, timespec_va);
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
    size_t mountflags;
    argaddr(idx++, &mountflags);

    // parameter 4: const void *data
    size_t addr_data;
    argaddr(idx++, &addr_data);

    return mount(source, target, filesystemtype, (unsigned long)mountflags,
                 addr_data);
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
