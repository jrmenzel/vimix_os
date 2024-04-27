/* SPDX-License-Identifier: MIT */

#include <syscalls/syscall.h>

#include <arch/trap.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

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
    size_t p;
    argaddr(0, &p);
    return wait((int32_t *)p);
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

size_t sys_sleep()
{
    // parameter 0: seconds
    int n;
    argint(0, &n);
    spin_lock(&g_tickslock);
    size_t ticks0 = g_ticks;
    while (g_ticks - ticks0 < n)
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
    int pid;

    argint(0, &pid);
    return proc_kill(pid);
}
