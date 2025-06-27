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

#define MAX_PROC_DEBUG_NAME 16

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
    size_t kstack;  ///< Virtual address of kernel stack

    // Process memory starts at USER_TEXT_START with the binary, data, bss etc.
    size_t heap_begin;  ///< at runtime allocated data (by sbrk()) starts here
    size_t heap_end;    ///< heap_end
    size_t stack_low;   ///< First/lowest stack page address. Stack goes to
                       ///< USER_STACK_HIGH - 1 and sp starts at USER_STACK_HIGH
    pagetable_t pagetable;        ///< User page table
    struct trapframe *trapframe;  ///< data page for u_mode_trap_vector.S
    struct context context;       ///< context_switch() here to run process
    struct file *files[MAX_FILES_PER_PROCESS];  ///< Open files. Indexed by a
                                                ///< FILE_DESCRIPTOR value.
    struct inode *cwd;                          ///< Current Working Directory

    char name[MAX_PROC_DEBUG_NAME];  ///< Process name (debugging)
#ifdef CONFIG_DEBUG
    size_t current_syscall;  ///< More info in process listing via CTRL+P
#endif

    int32_t debug_log_depth;  // debug
};

/// List of all user processes.
extern struct process g_process_list[MAX_PROCS];
