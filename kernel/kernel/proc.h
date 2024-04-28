/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/context.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>

/// Per-CPU state.
struct cpu
{
    struct process *proc;    ///< The process running on this cpu, or null.
    struct context context;  ///< context_switch() here to enter scheduler().
    int disable_dev_int_stack_depth;  ///< Depth of
                                      ///< cpu_push_disable_device_interrupt_stack()
                                      ///< nesting.
    int disable_dev_int_stack_original_state;  ///< Were interrupts enabled
                                               ///< before
                                               ///< cpu_push_disable_device_interrupt_stack()?
};

extern struct cpu g_cpus[MAX_CPUS];

/// @brief Exit process.
/// @param status Exit code ( return value from main() )
void exit(int32_t status);

/// @brief Create a new process, copying the parent.
/// Sets up child kernel stack to return as if from fork() system call.
int32_t fork();

/// @brief Grow or shrink user memory by n bytes.
/// @param n bytes to grow/shrink
/// @return 0 on success, -1 on failure.
int32_t proc_grow_memory(ssize_t n);

/// Allocate a page for each process's kernel stack.
void init_per_process_kernel_stack(pagetable_t kpage_table);

pagetable_t proc_pagetable(struct process *proc);

void proc_free_pagetable(pagetable_t pagetable, size_t sz);

/// @brief Sends a signal to a process, basically kill syscall
/// @param pid The PID of the process
/// @param sig The signal as defined in signal.h
int32_t proc_send_signal(pid_t pid, int32_t sig);

/// true if the process has been killed
bool proc_is_killed(struct process *proc);

/// kill the process
void proc_set_killed(struct process *proc);

struct cpu *get_cpu();

/// get currently running process
struct process *get_current();

/// initialize the process table.
void proc_init();

void sched();

void sleep(void *chan, struct spinlock *lk);

void userspace_init();

/// @brief Wait for a child process to exit and return its pid.
/// exposed via sys/wait.h
/// @param wstatus address of an int to store wstatus into
/// @return -1 if this process has no children.
extern pid_t wait(int32_t *wstatus);

void wakeup(void *chan);

void yield();

/// @brief Copy to either a user address, or kernel address, depending on
/// dst_addr_is_userspace.
/// @param dst_addr_is_userspace dst address is either in the users or kernels
/// addr space
/// @param dst_addr destination pointer
/// @param src source to copy from
/// @param len bytes to copy
/// @return 0 on success, -1 on error
int32_t either_copyout(bool addr_is_userspace, size_t dst, void *src,
                       size_t len);

/// @brief Copy from either a user address, or kernel address, depending on
/// src_addr_is_userspace.
/// @param dst destination to copy to
/// @param src_addr_is_userspace source address is either in the users or
/// kernels addr space
/// @param src_addr source pointer
/// @param len bytes to copy
/// @return 0 on success, -1 on error.
int32_t either_copyin(void *dst, bool addr_is_userspace, size_t src,
                      size_t len);

/// Prints the process list to the console (wired to CTRL+P)
void debug_print_process_list();

/// @brief Allocate a file descriptor for the given file and add it to the
/// current process.
/// @param f the file
/// @return the file descriptor or -1 on failure
FILE_DESCRIPTOR fd_alloc(struct file *f);
