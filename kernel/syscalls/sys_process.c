/* SPDX-License-Identifier: MIT */

#include <arch/timer.h>
#include <arch/trap.h>
#include <kernel/exec.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/limits.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/kalloc.h>
#include <syscalls/syscall.h>

ssize_t sys_exit()
{
    // parameter 0: int32_t status
    int32_t status;
    argint(0, &status);

    do_exit(status);
    return 0;  // not reached
}

ssize_t sys_getpid() { return get_current()->pid; }

ssize_t sys_fork() { return do_fork(); }

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
    intptr_t increment;
    argssize_t(0, &increment);

    size_t addr = get_current()->heap_end;
    if (proc_grow_memory(increment) < 0)
    {
        // TODO: the partial allocation is cleared, but the page table might
        // have gotton new pages (with invalid entries). So some memory is
        // wasted until the process is killed.
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
        argv[i] = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
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
        free_page(argv[i]);
    }
    return error_code;
}

ssize_t do_getgroups(int32_t size, size_t list_addr)
{
    struct process *proc = get_current();

    if (size == 0)
    {
        // return group count
        return proc->cred.groups->ngroups;
    }

    if (size < proc->cred.groups->ngroups)
    {
        return -EINVAL;
    }

    if (size > NGROUPS_MAX)
    {
        return -EINVAL;
    }

    size_t to_copy = proc->cred.groups->ngroups;
    if (uvm_copy_out(proc->pagetable, list_addr, (char *)proc->cred.groups->gid,
                     to_copy * sizeof(gid_t)) < 0)
    {
        return -EFAULT;
    }

    return to_copy;
}

ssize_t sys_getgroups()
{
    // parameter 0: int size
    int32_t size;
    argint(0, &size);

    // parameter 1: gid_t *list
    size_t list_addr;
    argaddr(1, &list_addr);

    return do_getgroups(size, list_addr);
}

ssize_t do_setgroups(int32_t size, size_t list_addr)
{
    struct process *proc = get_current();

    if (IS_NOT_SUPERUSER(&proc->cred))
    {
        return -EPERM;
    }

    if (size == 0)
    {
        // return group count
        return proc->cred.groups->ngroups;
    }

    if (size > NGROUPS_MAX)
    {
        return -EINVAL;
    }

    struct group_info *new_groups = groups_alloc(size);
    if (new_groups == NULL)
    {
        return -ENOMEM;
    }

    size_t to_copy = size;

    if (uvm_copy_in(proc->pagetable, (char *)new_groups->gid, list_addr,
                    to_copy * sizeof(gid_t)) < 0)
    {
        return -EFAULT;
    }

    if (proc->cred.groups != NULL)
    {
        put_group_info(proc->cred.groups);
    }
    proc->cred.groups = new_groups;  // stil ref count 1 from alloc

    return 0;
}

ssize_t sys_setgroups()
{
    // parameter 0: int size
    int32_t size;
    argint(0, &size);

    // parameter 1: gid_t *list
    size_t list_addr;
    argaddr(1, &list_addr);

    return do_setgroups(size, list_addr);
}
