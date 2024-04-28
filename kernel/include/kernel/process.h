/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/context.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>

/// State of a process (sleeping, runnable, etc.)
enum process_state
{
    /// Indicates that the `process` does not hold a valid process
    UNUSED,
    /// State of a new process which isn't fully setup
    USED,
    /// Sleeping, via `sleep()`
    SLEEPING,
    /// Can be scheduled.
    RUNNABLE,
    /// Running.
    RUNNING,
    /// Process called `exit()`, process remains until parent process called
    /// `wait()`.
    ZOMBIE
};

/// Per-process state
/// Central struct to schedule processes.
///
/// Freed at free_process()
struct process
{
    struct spinlock lock;  ///< Access lock for this process

    // process->lock must be held when using these:
    enum process_state state;  ///< Process state
    void *chan;                ///< If non-zero, sleeping on chan
    bool killed;               ///< Has been killed?
    int32_t xstate;            ///< Exit status to be returned to parent's wait
    pid_t pid;                 ///< Process ID

    // g_wait_lock must be held when using this:
    struct process *parent;  ///< Parent process

    // these are private to the process, so process->lock need not be held.
    size_t kstack;                ///< Virtual address of kernel stack
    size_t sz;                    ///< Size of process memory (bytes)
    pagetable_t pagetable;        ///< User page table
    struct trapframe *trapframe;  ///< data page for u_mode_trap_vector.S
    struct context context;       ///< context_switch() here to run process
    struct file *files[MAX_FILES_PER_PROCESS];  ///< Open files. Indexed by a
                                                ///< FILE_DESCRIPTOR value.
    struct inode *cwd;                          ///< Current Working Directory

    char name[16];  ///< Process name (debugging)
};

/// List of all user processes.
extern struct process g_process_list[MAX_PROCS];
