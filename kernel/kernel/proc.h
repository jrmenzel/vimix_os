/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>
#include <mm/vm.h>

extern atomic_size_t g_kernel_pgtable_shootdown_epoch;

/// @brief Exit process.
/// @param status Exit code ( return value from main() )
void do_exit(int32_t status);

/// @brief Create a new process, copying the parent.
/// Sets up child kernel stack to return as if from fork() system call.
syserr_t do_fork();

/// @brief Grow or shrink user memory by n bytes.
/// @param n bytes to grow/shrink
/// @return 0 on success, -1 on failure.
int32_t proc_grow_memory(ssize_t n);

struct Page_Table *proc_pagetable(struct process *proc,
                                  bool create_kernel_stack);

/// @brief Sends a signal to a process, basically kill syscall
/// @param pid The PID of the process
/// @param sig The signal as defined in signal.h
syserr_t proc_send_signal(pid_t pid, int32_t sig);

/// true if the process has been killed
bool proc_is_killed(struct process *proc);

/// kill the process
void proc_set_killed(struct process *proc);

/// @brief Grow procs stack by one page.
/// @return true on success
bool proc_grow_stack(struct process *proc);

/// @brief Tries to shrink the stack if pages are unused to free them
void proc_shrink_stack(struct process *proc);

__attribute__((returns_nonnull)) struct cpu *get_cpu();

/// get currently running process
struct process *get_current();

/// initialize the process table.
void proc_init();

void context_switch_to_scheduler();

/// @brief Atomically release lock (if not NULL) and sleep on chan.
/// Reacquires lock (if not NULL) when awakened.
/// @param chan The channel to sleep on.
/// @param lk Lock to release before sleeping and reacquire after. Can be NULL.
void sleep(void *channel, struct spinlock *lk);

void init_userspace();

/// @brief Wait for a child process to exit and return its pid.
/// exposed via sys/wait.h
/// @param wstatus address of an int to store wstatus into
/// @return -1 if this process has no children.
syserr_t do_wait(int32_t *wstatus);

/// @brief Wake up all processes sleeping on channel chan.
/// Must be called without any proc->lock.
/// @param chan The channel to wake up.
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

static inline void proc_get(struct process *proc) { kobject_get(&proc->kobj); }

static inline void proc_put(struct process *proc) { kobject_put(&proc->kobj); }

/// @brief Frees all allocated memory of a process and the process struct itself
/// @param proc Process, lock must be held.
void process_free(struct process *proc);

/// @brief Prints the processes kernel call stack.
/// @param proc A not running process.
void debug_print_call_stack_kernel(struct process *proc);

/// @brief Prints the processes user call stack. This shows where an exception
/// happened but also where the app was before calling a syscall.
/// @param proc A not running process.
void debug_print_call_stack_user(struct process *proc);

/// @brief Prints the process list to the console (wired to CTRL+P)
/// Does not lock the process list for debugging a stuck system.
/// @param print_call_stack_user Prints the user space call stack of stopped
/// @param print_call_stack_kernel Prints the kernel space call stack of stopped
/// processes
/// @param print_files Print open files
/// @param print_page_table Prints the process page table
void debug_print_process_list(bool print_call_stack_user,
                              bool print_call_stack_kernel, bool print_files,
                              bool print_page_table);

/// @brief Allocate a file descriptor for the given file and add it to the
/// current process.
/// @param f the file
/// @return the file descriptor or -1 on failure
FILE_DESCRIPTOR fd_alloc(struct file *f);

void forkret();

size_t proc_get_free_kernel_stack_va();
