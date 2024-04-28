/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <arch/trap.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

size_t sys_exit()
{
    // parameter 0: int32_t status
    int32_t status;
    argint(0, &status);

    exit(status);
    return 0;  // not reached
}

size_t sys_getpid() { return get_current()->pid; }

size_t sys_fork() { return fork(); }

size_t sys_wait()
{
    // parameter 0: int32_t *wstatus
    size_t wstatus;
    argaddr(0, &wstatus);
    return wait((int32_t *)wstatus);
}

size_t sys_sbrk()
{
    size_t addr;
    int n;

    argint(0, &n);
    addr = get_current()->sz;
    if (proc_grow_memory(n) < 0) return -1;
    return addr;
}

size_t sys_ms_sleep()
{
    // parameter 0: milli_seconds
    int32_t milli_seconds;
    argint(0, &milli_seconds);

    size_t kernel_ticks = milli_seconds * TIMER_INTERRUPTS_PER_SECOND / 1000;

    spin_lock(&g_tickslock);
    size_t ticks0 = g_ticks;
    while (g_ticks - ticks0 < kernel_ticks)
    {
        if (proc_is_killed(get_current()))
        {
            spin_unlock(&g_tickslock);
            return -1;
        }
        sleep(&g_ticks, &g_tickslock);
    }
    spin_unlock(&g_tickslock);
    return 0;
}

size_t sys_kill()
{
    // parameter 0: pid
    pid_t pid;
    argint(0, &pid);

    // parameter 1: signal
    int32_t signal;
    argint(1, &signal);

    return proc_send_signal(pid, signal);
}
