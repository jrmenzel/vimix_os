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

/// Fetch the uint64_t at addr from the current process.
int fetchaddr(uint64_t addr, uint64_t *ip)
{
    struct process *proc = get_current();
    if (addr >= proc->sz ||
        addr + sizeof(uint64_t) >
            proc->sz)  // both tests needed, in case of overflow
        return -1;
    if (uvm_copy_in(proc->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
        return -1;
    return 0;
}

/// Fetch the nul-terminated string at addr from the current process.
/// Returns length of string, not including nul, or -1 for error.
int fetchstr(uint64_t addr, char *buf, int max)
{
    struct process *proc = get_current();
    if (uvm_copy_in_str(proc->pagetable, buf, addr, max) < 0) return -1;
    return strlen(buf);
}

static uint64_t argraw(int n)
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
    panic("argraw");
    return -1;
}

/// Fetch the nth 32-bit system call argument.
void argint(int n, int *ip) { *ip = argraw(n); }

/// Retrieve an argument as a pointer.
/// Doesn't check for legality, since
/// uvm_copy_in/uvm_copy_out will do that.
void argaddr(int n, uint64_t *ip) { *ip = argraw(n); }

/// Fetch the nth word-sized system call argument as a null-terminated string.
/// Copies into buf, at most max.
/// Returns string length if OK (including nul), -1 if error.
int argstr(int n, char *buf, int max)
{
    uint64_t addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64_t sys_fork();
extern uint64_t sys_exit();
extern uint64_t sys_wait();
extern uint64_t sys_pipe();
extern uint64_t sys_read();
extern uint64_t sys_kill();
extern uint64_t sys_execv();
extern uint64_t sys_fstat();
extern uint64_t sys_chdir();
extern uint64_t sys_dup();
extern uint64_t sys_getpid();
extern uint64_t sys_sbrk();
extern uint64_t sys_sleep();
extern uint64_t sys_uptime();
extern uint64_t sys_open();
extern uint64_t sys_write();
extern uint64_t sys_mknod();
extern uint64_t sys_unlink();
extern uint64_t sys_link();
extern uint64_t sys_mkdir();
extern uint64_t sys_close();

// clang-format off
/// An array mapping syscall numbers from syscall.h
/// to the function that handles the system call.
static uint64_t (*syscalls[])() = {
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
    int num;
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
