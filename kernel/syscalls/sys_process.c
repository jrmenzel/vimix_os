/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <arch/trap.h>
#include <kernel/exec.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/limits.h>
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
    // parameter 0: intptr_t increment
    size_t increment;
    argaddr(0, &increment);

    size_t addr = get_current()->sz;
    if (proc_grow_memory(increment) < 0)
    {
        return -1;
    }
    return addr;
}

size_t sys_ms_sleep()
{
    // parameter 0: milli_seconds
    int32_t milli_seconds;
    argint(0, &milli_seconds);

    if (milli_seconds < 0) milli_seconds = 0;

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

size_t sys_execv()
{
    // parameter 0: char *pathname
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -1;
    }

    // parameter 1: char *argv[]
    char *argv[MAX_EXEC_ARGS];
    memset(argv, 0, sizeof(argv));

    size_t uargv, uarg;
    argaddr(1, &uargv);

    for (size_t i = 0;; i++)
    {
        if (i >= NELEM(argv))
        {
            goto bad;
        }
        if (fetchaddr(uargv + sizeof(size_t) * i, (size_t *)&uarg) < 0)
        {
            goto bad;
        }
        if (uarg == 0)
        {
            argv[i] = 0;
            break;
        }
        argv[i] = kalloc();
        if (argv[i] == NULL) goto bad;
        if (fetchstr(uarg, argv[i], PAGE_SIZE) < 0) goto bad;
    }

    size_t ret = execv(path, argv);

    for (size_t i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    {
        kfree(argv[i]);
    }
    return ret;

bad:
    for (size_t i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    {
        kfree(argv[i]);
    }
    return -1;
}
