/* SPDX-License-Identifier: MIT */

#include <arch/timer.h>
#include <arch/trap.h>
#include <kernel/exec.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/limits.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <syscalls/syscall.h>

ssize_t sys_exit()
{
    // parameter 0: int32_t status
    int32_t status;
    argint(0, &status);

    exit(status);
    return 0;  // not reached
}

ssize_t sys_getpid() { return get_current()->pid; }

ssize_t sys_fork() { return fork(); }

ssize_t sys_wait()
{
    // parameter 0: int32_t *wstatus
    size_t wstatus;
    argaddr(0, &wstatus);
    return wait((int32_t *)wstatus);
}

ssize_t sys_sbrk()
{
    // parameter 0: intptr_t increment
    size_t increment;
    argaddr(0, &increment);

    size_t addr = get_current()->heap_end;
    if (proc_grow_memory(increment) < 0)
    {
        return -ENOMEM;
    }
    return addr;
}

ssize_t sys_ms_sleep()
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
            return -ESRCH;
        }
        sleep(&g_ticks, &g_tickslock);
    }
    spin_unlock(&g_tickslock);
    return 0;
}

ssize_t sys_kill()
{
    // parameter 0: pid
    pid_t pid;
    argint(0, &pid);

    // parameter 1: signal
    int32_t signal;
    argint(1, &signal);

    return proc_send_signal(pid, signal);
}

ssize_t sys_execv()
{
    // parameter 0: char *pathname
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: char *argv[]
    char *argv[MAX_EXEC_ARGS];
    memset(argv, 0, sizeof(argv));

    size_t uargv, uarg;
    argaddr(1, &uargv);

    ssize_t error_code = 0;
    for (size_t i = 0;; i++)
    {
        if (i >= NELEM(argv))
        {
            error_code = -E2BIG;
            break;
        }
        if (fetchaddr(uargv + sizeof(size_t) * i, (size_t *)&uarg) < 0)
        {
            error_code = -EFAULT;
            break;
        }
        if (uarg == 0)
        {
            argv[i] = 0;
            break;
        }
        argv[i] = kalloc();
        if (argv[i] == NULL)
        {
            error_code = -ENOMEM;
            break;
        }
        if (fetchstr(uarg, argv[i], PAGE_SIZE) < 0)
        {
            error_code = -EFAULT;
            break;
        }
    }

    if (error_code == 0)
    {
        // printk("execv(%s)\n", path);
        error_code = execv(path, argv);
        // printk("execv(%s) = %zd\n", path, error_code);
    }
    // cleanup on error and success:
    for (size_t i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    {
        kfree(argv[i]);
    }
    return error_code;
}
