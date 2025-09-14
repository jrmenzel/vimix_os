/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/context.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>
#include <kernel/rwspinlock.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>
#include <lib/bitmap.h>

/// State of a process (sleeping, runnable, etc.)
enum process_state
{
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

/// @brief There is one global process list g_process_list
struct process_list
{
    struct list_head plist;  ///< list of all processes
    struct rwspinlock lock;  ///< access lock for all read/write accesses to the
                             ///< process linked list.
    bitmap_t kernel_stack_in_use;  ///< keeps track which addresses are in use
    struct spinlock kernel_stack_lock;  ///< lock for kernel_stack_in_use
};

/// Per-process state
/// Central struct to schedule processes.
///
/// Freed at proc_free()
struct process
{
    struct kobject kobj;     ///< Kernel object for this process
    struct list_head plist;  ///< double linked list of all processes

    struct spinlock lock;  ///< Access lock for this process (except the list
                           ///< plist of all processes)

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

extern struct process_list g_process_list;

#define process_from_list(ptr) container_of(ptr, struct process, plist)
#define process_from_kobj(ptr) container_of(ptr, struct process, kobj)
