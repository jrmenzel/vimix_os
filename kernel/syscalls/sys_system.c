/* SPDX-License-Identifier: MIT */

//
// System Information system calls.
//

#include <arch/trap.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/reboot.h>
#include <kernel/reset.h>
#include <kernel/rtc.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>
#include <syscalls/syscall.h>

syserr_t sys_uptime() { return (syserr_t)kticks_get_ticks(); }

void debug_print_cpu_states()
{
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if (g_cpus[i].state == CPU_UNUSED) continue;
        const char *state_str = "unknown";
        switch (g_cpus[i].state)
        {
            case CPU_UNUSED: state_str = "unused"; break;
            case CPU_STARTED: state_str = "started"; break;
            case CPU_HALTED: state_str = "halted"; break;
            case CPU_PANICKED: state_str = "panicked"; break;
            default: break;
        }
        printk("CPU %zd: state %s %s\n", i, state_str,
               (i == smp_processor_id()) ? "(current CPU)" : "");
    }
}

void system_shutdown()
{
    // stop other CPUs
    cpu_mask mask = ipi_cpu_mask_all_but_self();
    ipi_send_interrupt(mask, IPI_SHUTDOWN, NULL);

    struct timespec t0 = rtc_get_time();

    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if (i == smp_processor_id()) continue;
        if (g_cpus[i].state == CPU_UNUSED) continue;

        while (g_cpus[i].state != CPU_HALTED && g_cpus[i].state != CPU_PANICKED)
        {
            // wait for other existing (!CPU_UNUSED) CPUs to halt or panic
            atomic_thread_fence(memory_order_seq_cst);

            struct timespec t1 = rtc_get_time();
            if (t1.tv_sec - t0.tv_sec > 3)
            {
                printk(
                    "Not all CPUs halted within 3 seconds, proceeding with "
                    "shutdown\n");
                debug_print_cpu_states();
                return;
            }
        }
    }

    printk("All other CPUs halted.\n");
}

syserr_t sys_reboot()
{
    // parameter 0: cmd
    int32_t cmd;
    argint(0, &cmd);

    // this syscall is for root only
    struct process *proc = get_current();
    if (IS_NOT_SUPERUSER(&proc->cred))
    {
        return -EPERM;
    }

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

syserr_t get_time_to_user(clockid_t clockid, size_t timespec_va)
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

syserr_t sys_clock_gettime()
{
    // parameter 0: cklockid
    clockid_t clock;
    argint(0, &clock);

    // parameter 1: timespec *tp
    size_t timespec_va;
    argaddr(1, &timespec_va);

    return get_time_to_user(clock, timespec_va);
}
