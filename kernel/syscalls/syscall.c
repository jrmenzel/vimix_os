/* SPDX-License-Identifier: MIT */

#include <syscalls/syscall.h>

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/unistd.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

int32_t fetchaddr(size_t addr, size_t *ip)
{
    struct process *proc = get_current();
    if (addr >= proc->sz ||
        addr + sizeof(size_t) >
            proc->sz)  // both tests needed, in case of overflow
        return -1;
    if (uvm_copy_in(proc->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
        return -1;
    return 0;
}

int32_t fetchstr(size_t addr, char *buf, int32_t max)
{
    struct process *proc = get_current();
    if (uvm_copy_in_str(proc->pagetable, buf, addr, max) < 0) return -1;
    return strlen(buf);
}

static size_t get_system_call_argument(int n)
{
    struct process *proc = get_current();
    switch (n)
    {
        case 0: return proc->trapframe->a0;
        case 1: return proc->trapframe->a1;
        case 2: return proc->trapframe->a2;
        case 3: return proc->trapframe->a3;
        case 4: return proc->trapframe->a4;
        case 5: return proc->trapframe->a5;
    }
    panic("get_system_call_argument");
    return -1;
}

void argint(int n, int *ip) { *ip = get_system_call_argument(n); }

void argaddr(int n, size_t *ip) { *ip = get_system_call_argument(n); }

int32_t argstr(int32_t n, char *buf, int32_t max)
{
    size_t addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, max);
}

// clang-format off
/// An array mapping syscall numbers from syscall.h
/// to the function that handles the system call.
static size_t (*syscalls[])() = {
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
    [SYS_sleep] sys_sleep,
    [SYS_uptime] sys_uptime,
    [SYS_open] sys_open,
    [SYS_write] sys_write,
    [SYS_mknod] sys_mknod,
    [SYS_unlink] sys_unlink,
    [SYS_link] sys_link,
    [SYS_mkdir] sys_mkdir,
    [SYS_close] sys_close,
};
// clang-format on

void syscall()
{
    size_t num;
    struct process *proc = get_current();

    num = proc->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num])
    {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in proc->trapframe->a0
        proc->trapframe->a0 = syscalls[num]();
    }
    else
    {
        printk("%d %s: unknown sys call %d\n", proc->pid, proc->name, num);
        proc->trapframe->a0 = -1;
    }
}
