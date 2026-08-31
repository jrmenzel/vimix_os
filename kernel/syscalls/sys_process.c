/* SPDX-License-Identifier: MIT */

#include <arch/trap.h>
#include <fs/dentry_cache.h>
#include <kernel/exec.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/limits.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/timer.h>
#include <mm/kalloc.h>
#include <syscalls/syscall.h>

syserr_t sys_exit()
{
    // parameter 0: int32_t status
    int32_t status;
    argint(0, &status);

    do_exit(status);
    return 0;  // not reached
}

syserr_t sys_getpid() { return (syserr_t)get_current()->pid; }

syserr_t sys_fork() { return do_fork(); }

syserr_t sys_wait()
{
    // parameter 0: int32_t *wstatus
    size_t wstatus;
    argaddr(0, &wstatus);
    return do_wait((int32_t *)wstatus);
}

syserr_t sys_sbrk()
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

    return (syserr_t)addr;
}

syserr_t sys_ms_sleep()
{
    // parameter 0: milli_seconds
    int32_t milli_seconds;
    argint(0, &milli_seconds);

    if (milli_seconds < 0) milli_seconds = 0;

    size_t kernel_ticks = milli_seconds * TIMER_INTERRUPTS_PER_SECOND / 1000;

    size_t ticks0 = atomic_load(&g_ticks);
    while (atomic_load(&g_ticks) - ticks0 < kernel_ticks)
    {
        if (proc_is_killed(get_current()))
        {
            return -ESRCH;
        }
        sleep(&g_ticks, NULL);
    }
    return 0;
}

syserr_t sys_kill()
{
    // parameter 0: pid
    pid_t pid;
    argint(0, &pid);

    // parameter 1: signal
    int32_t signal;
    argint(1, &signal);

    return proc_send_signal(pid, signal);
}

syserr_t sys_execv()
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

    syserr_t error_code = 0;
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
        error_code = do_execv(path, argv);
    }
    // cleanup on error and success:
    for (size_t i = 0; i < NELEM(argv) && argv[i] != NULL; i++)
    {
        free_page(argv[i]);
    }
    return error_code;
}

syserr_t do_getgroups(int32_t size, size_t list_addr)
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

    return (syserr_t)to_copy;
}

syserr_t sys_getgroups()
{
    // parameter 0: int size
    int32_t size;
    argint(0, &size);

    // parameter 1: gid_t *list
    size_t list_addr;
    argaddr(1, &list_addr);

    return do_getgroups(size, list_addr);
}

syserr_t do_setgroups(int32_t size, size_t list_addr)
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

syserr_t sys_setgroups()
{
    // parameter 0: int size
    int32_t size;
    argint(0, &size);

    // parameter 1: gid_t *list
    size_t list_addr;
    argaddr(1, &list_addr);

    return do_setgroups(size, list_addr);
}

syserr_t do_getcwdlen_locked(size_t buf_addr, size_t buf_size,
                             struct dentry *dentry)
{
    if (dentry_is_unlinked(dentry))
    {
        return -ENOENT;
    }

    size_t required_len = dentry_get_cwd_length(dentry);

    if (buf_addr == 0)
    {
        // only query the length
        return required_len;
    }

    if (buf_size < required_len)
    {
        return -ERANGE;
    }

    // try to copy the path

    // sanity check:
    if (required_len > PAGE_SIZE)
    {
        return -EOTHER;
    }

    char *tmp = kmalloc(required_len, ALLOC_FLAG_ZERO_MEMORY);
    if (tmp == NULL)
    {
        return -ENOMEM;
    }

    dentry_get_cwd(dentry, tmp, required_len);

    struct process *proc = get_current();
    if (uvm_copy_out(proc->pagetable, buf_addr, tmp, required_len) < 0)
    {
        kfree(tmp);
        return -EFAULT;
    }

    kfree(tmp);

    return required_len;
}

syserr_t do_getcwdlen(size_t buf_addr, size_t buf_size)
{
    struct process *proc = get_current();

    if ((buf_addr != 0) && (buf_size == 0))
    {
        return -EINVAL;
    }

    dcache_read_lock();
    syserr_t ret = do_getcwdlen_locked(buf_addr, buf_size, proc->cwd_dentry);
    dcache_read_unlock();

    return ret;
}

syserr_t sys_getcwdlen()
{
    // parameter 0: char buf[]
    size_t buf_addr;
    argaddr(0, &buf_addr);

    // parameter 1: size_t size
    size_t buf_size;
    argsize_t(1, &buf_size);

    return do_getcwdlen(buf_addr, buf_size);
}
