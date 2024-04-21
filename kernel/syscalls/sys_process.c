/* SPDX-License-Identifier: MIT */

#include <syscalls/syscall.h>

#include <arch/trap.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

uint64 sys_exit()
{
    int n;
    argint(0, &n);
    exit(n);
    return 0;  // not reached
}

uint64 sys_getpid() { return get_current()->pid; }

uint64 sys_fork() { return fork(); }

uint64 sys_wait()
{
    uint64 p;
    argaddr(0, &p);
    return wait(p);
}

uint64 sys_sbrk()
{
    uint64 addr;
    int n;

    argint(0, &n);
    addr = get_current()->sz;
    if (proc_grow_memory(n) < 0) return -1;
    return addr;
}

uint64 sys_sleep()
{
    int n;
    uint ticks0;

    argint(0, &n);
    spin_lock(&g_tickslock);
    ticks0 = g_ticks;
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

uint64 sys_kill()
{
    int pid;

    argint(0, &pid);
    return proc_kill(pid);
}
