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
    struct process *proc;    // The process running on this cpu, or null.
    struct context context;  // context_switch() here to enter scheduler().
    int disable_dev_int_stack_depth;  // Depth of
                                      // cpu_push_disable_device_interrupt_stack()
                                      // nesting.
    int disable_dev_int_stack_original_state;  // Were interrupts enabled before
                                               // cpu_push_disable_device_interrupt_stack()?
};

extern struct cpu g_cpus[MAX_CPUS];

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
struct process
{
    struct spinlock lock;

    // p->lock must be held when using these:
    enum procstate state;  ///< Process state
    void *chan;            ///< If non-zero, sleeping on chan
    int killed;            ///< If non-zero, have been killed
    int xstate;            ///< Exit status to be returned to parent's wait
    int pid;               ///< Process ID

    // wait_lock must be held when using this:
    struct process *parent;  ///< Parent process

    // these are private to the process, so p->lock need not be held.
    uint64_t kstack;              ///< Virtual address of kernel stack
    uint64_t sz;                  ///< Size of process memory (bytes)
    pagetable_t pagetable;        ///< User page table
    struct trapframe *trapframe;  ///< data page for u_mode_trap_vector.S
    struct context context;       ///< context_switch() here to run process
    struct file *files[NOFILE];   ///< Open files
    struct inode *cwd;            ///< Current directory
    char name[16];                ///< Process name (debugging)
};

int smp_processor_id();
void exit(int);
int fork();
int proc_grow_memory(int);
void init_per_process_kernel_stack(pagetable_t);
pagetable_t proc_pagetable(struct process *);
void proc_free_pagetable(pagetable_t, uint64_t);
int proc_kill(int);
int proc_is_killed(struct process *);
void proc_set_killed(struct process *);
struct cpu *get_cpu();
struct process *get_current();
void proc_init();
void sched();
void sleep(void *, struct spinlock *);
void userspace_init();
int wait(uint64_t);
void wakeup(void *);
void yield();
int either_copyout(int addr_is_userspace, uint64_t dst, void *src,
                   uint64_t len);
int either_copyin(void *dst, int addr_is_userspace, uint64_t src, uint64_t len);
void debug_print_process_list();

int fd_alloc(struct file *f);
