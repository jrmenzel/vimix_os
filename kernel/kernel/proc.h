/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/context.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>

/// Per-CPU state.
struct cpu
{
    struct proc *proc;       // The process running on this cpu, or null.
    struct context context;  // swtch() here to enter scheduler().
    int noff;                // Depth of push_off() nesting.
    int intena;              // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

enum procstate
{
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};

/// Per-process state
struct proc
{
    struct spinlock lock;

    // p->lock must be held when using these:
    enum procstate state;  ///< Process state
    void *chan;            ///< If non-zero, sleeping on chan
    int killed;            ///< If non-zero, have been killed
    int xstate;            ///< Exit status to be returned to parent's wait
    int pid;               ///< Process ID

    // wait_lock must be held when using this:
    struct proc *parent;  ///< Parent process

    // these are private to the process, so p->lock need not be held.
    uint64 kstack;                ///< Virtual address of kernel stack
    uint64 sz;                    ///< Size of process memory (bytes)
    pagetable_t pagetable;        ///< User page table
    struct trapframe *trapframe;  ///< data page for trampoline.S
    struct context context;       ///< swtch() here to run process
    struct file *ofile[NOFILE];   ///< Open files
    struct inode *cwd;            ///< Current directory
    char name[16];                ///< Process name (debugging)
};

int cpuid(void);
void exit(int);
int fork(void);
int growproc(int);
void proc_mapstacks(pagetable_t);
pagetable_t proc_pagetable(struct proc *);
void proc_freepagetable(pagetable_t, uint64);
int kill(int);
int killed(struct proc *);
void setkilled(struct proc *);
struct cpu *mycpu(void);
struct cpu *getmycpu(void);
struct proc *myproc();
void procinit(void);
void sched(void);
void sleep(void *, struct spinlock *);
void userinit(void);
int wait(uint64);
void wakeup(void *);
void yield(void);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void procdump(void);

int fdalloc(struct file *f);
