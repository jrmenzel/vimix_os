/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/errno.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

/// @brief Gets the syscall number from the processes trapframe and
/// calls the matching syscall. Also sets return value to trapframe.
/// Called by the interrupt vector user_mode_interrupt_handler() for all
/// syscalls.
void syscall(struct process *proc);

// ********************************************************
// Process control from sys_process.c
//

/// @brief Syscall "pid_t fork()" from uinstd.h.
/// @return Child PID to parent and 0 to child.
ssize_t sys_fork();

/// @brief Syscall "int32_t execv(const char *pathname, char *argv[])" from
/// uinstd.h.
/// @return Does not return on success, -ERRNO on error.
ssize_t sys_execv();

/// @brief  Syscall "void exit(int32_t status)" from uinstd.h.
/// @return Does not return.
ssize_t sys_exit();

/// @brief Syscall "int32_t kill(pid_t pid, int sig)" from signal.h
ssize_t sys_kill();

/// @brief Syscall "int32_t ms_sleep(int32_t mseconds)" from uinstd.h.
/// Also int32_t sleep(seconds)
ssize_t sys_ms_sleep();

/// @brief Syscall "pid_t wait(int *wstatus)" from wait.h
ssize_t sys_wait();

/// @brief Syscall "int32_t chdir(const char *path)" from uinstd.h.
ssize_t sys_chdir();

/// @brief Syscall "void *sbrk(intptr_t increment)" from uinstd.h.
ssize_t sys_sbrk();

/// @brief Syscall "pid_t getpid()" from uinstd.h.
ssize_t sys_getpid();

// ********************************************************
// File management from sys_file.c
//

/// @brief Syscall "int32_t mkdir(const char *path, mode_t mode)" from stat.h.
ssize_t sys_mkdir();

/// @brief Syscall "int32_t mknod(const char *path, mode_t mode, dev_t dev)"
/// from stat.h.
ssize_t sys_mknod();

/// @brief Syscall "int open(const char *pathname, int32_t flags, ...)" from
/// fcntl.h.
ssize_t sys_open();

/// @brief Syscall "int32_t close(int fd)" from uinstd.h.
ssize_t sys_close();

/// @brief Syscall "ssize_t read(int fd, void *buffer, size_t n)" from uinstd.h.
ssize_t sys_read();

/// @brief Syscall "ssize_t write(int fd, const void *buffer, size_t n)" from
/// uinstd.h.
ssize_t sys_write();

/// @brief Syscall "int dup(int fd)" from uinstd.h.
ssize_t sys_dup();

/// @brief Syscall "int32_t link(const char *from, const char *to)" from
/// uinstd.h.
ssize_t sys_link();

/// @brief Syscall "int32_t unlink(const char *pathname)" from uinstd.h.
ssize_t sys_unlink();

/// @brief Syscall "int rmdir(const char *path)" from uinstd.h.
ssize_t sys_rmdir();

/// @brief Syscall "int32_t fstat(FILE_DESCRIPTOR fd, struct stat *buffer)" from
/// stat.h.
ssize_t sys_fstat();

/// @brief Syscall "ssize_t get_dirent(int fd, struct dirent *dirp, size_t
/// seek_pos);" from dirent.h
ssize_t sys_get_dirent();

/// @brief Syscall "extern off_t lseek(int fd, off_t offset, int whence);" in
/// ustd.h
ssize_t sys_lseek();

// ********************************************************
// System information and control from sys_system.c
//

/// @brief Syscall "int32_t uptime()" from uinstd.h.
/// @return how many clock tick interrupts have occurred
ssize_t sys_uptime();

/// @brief Syscall "ssize_t reboot(int32_t cmd)" from reboot.h
/// @return ideally not...
ssize_t sys_reboot();

/// @brief Syscall "ssize_t get_time(time_t *tloc);" (exposed via time() in
/// time.h)
/// @return 0 on success, -ERRNO on error
ssize_t sys_get_time();

/// @brief Syscall "int mount(const char *source, const char *target, const char
/// *filesystemtype, unsigned long mountflags, const void *data);" from mount.h
/// @return 0 on success, -ERRNO on error
ssize_t sys_mount();

/// @brief Syscall "int umount(const char *target);" from mount.h
/// @return 0 on success, -ERRNO on error
ssize_t sys_umount();

// ********************************************************
// IPC from sys_ipc.c
//

/// @brief Syscall "int32_t pipe(int pipe_descriptors[2])" from uinstd.h.
ssize_t sys_pipe();

// ********************************************************
// Device functions from sys_device.c
//

/// @brief Syscall "int ioctl(int fd, unsigned long request, ...)" from ioctl.h.
ssize_t sys_ioctl();

// ********************************************************
// Helper functions to get syscall parameters
//

/// @brief Fetch the nth 32-bit system call argument.
/// @return 0 on success, -1 on failure
int32_t argint(int32_t n, int32_t *ip);

/// @brief Fetch the nth 32-bit unsigned system call argument.
/// @return 0 on success, -1 on failure
uint32_t arguint(int32_t n, uint32_t *ip);

/// @brief Fetch the nth word-sized system call argument as a null-terminated
/// string. Copies into buf, at most max chars.
/// @return string length if OK (including nul), -1 if error.
int32_t argstr(int32_t n, char *buf, size_t max);

/// @brief Retrieve an argument as an ssize_t.
/// @return 0 on success, -1 on failure
int32_t argssize_t(int32_t n, ssize_t *ip);

/// @brief Retrieve an argument as a pointer.
/// Doesn't check for legality, since
/// uvm_copy_in/uvm_copy_out will do that.
/// @return 0 on success, -1 on failure
int32_t argaddr(int32_t n, size_t *ip);

/// @brief Fetch the nth word-sized system call argument as a file descriptor
/// and return both the descriptor and the corresponding struct file.
/// @return 0 on success, -1 on failure
int32_t argfd(int32_t n, int32_t *pfd, struct file **pf);

/// @brief Fetch the nul-terminated string at addr from the current process.
/// @return length of string, not including nul, or -1 for error.
size_t fetchstr(size_t addr, char *buf, size_t max);

/// @brief Fetch the size_t at addr from the current process.
/// @return 0 on success, -1 on failure
int32_t fetchaddr(size_t addr, size_t *ip);

#ifdef CONFIG_DEBUG
const char *debug_get_syscall_name(size_t number);
#endif
