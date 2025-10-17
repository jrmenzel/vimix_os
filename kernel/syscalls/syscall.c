/* SPDX-License-Identifier: MIT */

#include <arch/trapframe.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <kernel/unistd.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

bool addr_is_proc_owned(struct process *proc, size_t addr)
{
    // the heap starts right after the apps binary and data
    const size_t app_start = USER_TEXT_START;
    const size_t heap_end = proc->heap_end;

    const size_t stack_start = proc->stack_low;
    const size_t stack_end = USER_STACK_HIGH;

    if (addr >= app_start && addr < heap_end)
    {
        return true;
    }
    if (addr >= stack_start && addr < stack_end)
    {
        return true;
    }
    return false;
}

int32_t fetchaddr(size_t addr, size_t *ip)
{
    struct process *proc = get_current();

    if (!addr_is_proc_owned(proc, addr) ||
        !addr_is_proc_owned(proc, addr + sizeof(size_t)))
    {
        // both tests needed, in case of overflow
        return -1;
    }

    if (uvm_copy_in(proc->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    {
        return -1;
    }
    return 0;
}

size_t fetchstr(size_t addr, char *buf, size_t max)
{
    struct process *proc = get_current();
    if (uvm_copy_in_str(proc->pagetable, buf, addr, max) < 0)
    {
        return -1;
    }
    return strlen(buf);
}

// returns the value in the n-th system call argument register of the trapframe
static size_t get_system_call_argument(int n)
{
    struct process *proc = get_current();
    return trapframe_get_argument_register(proc->trapframe, n);
}

int32_t argint(int32_t n, int32_t *ip)
{
    *ip = (int32_t)get_system_call_argument(n);
    return 0;
}

uint32_t arguint(int32_t n, uint32_t *ip)
{
    *ip = (uint32_t)get_system_call_argument(n);
    return 0;
}

int32_t argaddr(int32_t n, size_t *ip)
{
    *ip = get_system_call_argument(n);
    return 0;
}

int32_t argssize_t(int32_t n, ssize_t *ip)
{
    *ip = (ssize_t)get_system_call_argument(n);
    return 0;
}

int32_t argsize_t(int32_t n, size_t *ip)
{
    *ip = (size_t)get_system_call_argument(n);
    return 0;
}

int32_t argstr(int32_t n, char *buf, size_t max)
{
    size_t addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, max);
}

int argfd(int n, int *pfd, struct file **pf)
{
    FILE_DESCRIPTOR fd;
    struct file *f;

    argint(n, &fd);
    struct process *proc = get_current();
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || (f = proc->files[fd]) == 0)
    {
        return -1;
    }

    if (pfd)
    {
        *pfd = fd;
    }

    if (pf)
    {
        *pf = f;
    }

    return 0;
}

// clang-format off
/// An array mapping syscall numbers from syscall.h
/// to the function that handles the system call.
static ssize_t (*syscalls[])() = {
    [SYS_fork] sys_fork,
    [SYS_exit] sys_exit,
    [SYS_wait] sys_wait,
    [SYS_pipe] sys_pipe,
    [SYS_read] sys_read,
    [SYS_kill] sys_kill,
    [SYS_execv] sys_execv,
    [SYS_fstat] sys_fstat,
    [SYS_chdir] sys_chdir,
    [SYS_dup] sys_dup,
    [SYS_getpid] sys_getpid,
    [SYS_sbrk] sys_sbrk,
    [SYS_ms_sleep] sys_ms_sleep,
    [SYS_uptime] sys_uptime,
    [SYS_open] sys_open,
    [SYS_write] sys_write,
    [SYS_mknod] sys_mknod,
    [SYS_unlink] sys_unlink,
    [SYS_link] sys_link,
    [SYS_mkdir] sys_mkdir,
    [SYS_close] sys_close,
    [SYS_get_dirent] sys_get_dirent,
    [SYS_reboot] sys_reboot,
    [SYS_clock_gettime] sys_clock_gettime,
    [SYS_lseek] sys_lseek,
    [SYS_rmdir] sys_rmdir,
    [SYS_mount] sys_mount,
    [SYS_umount] sys_umount,
    [SYS_ioctl] sys_ioctl,
    [SYS_statvfs] sys_statvfs,
    [SYS_fstatvfs] sys_fstatvfs,
    [SYS_truncate] sys_truncate,
    [SYS_ftruncate] sys_ftruncate
};
// clang-format on

void syscall(struct process *proc)
{
    size_t num = trapframe_get_sys_call_number(proc->trapframe);

    if (num > 0 && num < NELEM(syscalls) && syscalls[num])
    {
#ifdef CONFIG_DEBUG
        proc->current_syscall = num;
#endif
        // Use num to lookup the system call function for num, call it,
        // and store its return value in proc->trapframe->a0
        ssize_t syscall_return_value = syscalls[num]();

#ifdef CONFIG_DEBUG
        proc->current_syscall = 0;
#endif
        trapframe_set_return_register(proc->trapframe, syscall_return_value);
    }
    else
    {
        printk("%d %s: unknown sys call %zd\n", proc->pid, proc->name, num);
        trapframe_set_return_register(proc->trapframe, -EINVALSCALL);
    }
}

#ifdef CONFIG_DEBUG
// clang-format off
/// An array mapping syscall numbers from syscall.h
/// to the function that handles the system call.
static const char *g_syscall_names[] = {
    [SYS_fork] "fork",
    [SYS_exit] "exit",
    [SYS_wait] "wait",
    [SYS_pipe] "pipe",
    [SYS_read] "read",
    [SYS_kill] "kill",
    [SYS_execv] "execv",
    [SYS_fstat] "fstat",
    [SYS_chdir] "chdir",
    [SYS_dup] "dup",
    [SYS_getpid] "getpid",
    [SYS_sbrk] "sbrk",
    [SYS_ms_sleep] "ms_sleep",
    [SYS_uptime] "uptime",
    [SYS_open] "open",
    [SYS_write] "write",
    [SYS_mknod] "mknod",
    [SYS_unlink] "unlink",
    [SYS_link] "link",
    [SYS_mkdir] "mkdir",
    [SYS_close] "close",
    [SYS_get_dirent] "get_dirent",
    [SYS_reboot] "reboot",
    [SYS_clock_gettime] "clock_gettime",
    [SYS_lseek] "lseek",
    [SYS_rmdir] "rmdir",
    [SYS_mount] "sys_mount",
    [SYS_umount] "sys_umount",
    [SYS_ioctl] "ioctl",
    [SYS_statvfs] "statvfs",
    [SYS_fstatvfs] "fstatvfs",
    [SYS_truncate] "truncate",
    [SYS_ftruncate] "ftruncate"
};
// clang-format on

const char *debug_get_syscall_name(size_t number)
{
    if (number > 0 && number < NELEM(syscalls))
    {
        return g_syscall_names[number];
    }
    return "UNKNOWN";
}
#endif
