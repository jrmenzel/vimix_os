/* SPDX-License-Identifier: MIT */
#pragma once

// Unix standard API POSIX
//
// Mostly system call wrappers
// Additional system calls are in include/sys:
// - wait.h
// - signal.h
// - stat.h
// - fsctl.h
// ...

// must be included according to the POSIX standard, so why not :-)
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <kernel/unistd.h>
#include <sys/types.h>

// normally not included here, but fcntl.h contains open() while
// unistd.h contains close(), so you'll likely need both.
#include <fcntl.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

//
// unless mentioned otherwise: return 0 on success, -1 on failure.
//

///////////////////////////////////////
// 1. Process Control

// Clone the calling process, return the childs PID to the parent
// and 0 to the child.
extern pid_t fork();

// replace current process with one from path
extern int32_t execv(const char *pathname, char *argv[]);

// terminates program with status as return code
extern void exit(int32_t status) __attribute__((noreturn));

// get process ID
extern pid_t getpid();

extern ssize_t ms_sleep(int32_t milliseconds);

// let process sleep
static inline uint32_t sleep(int32_t seconds)
{
    return (uint32_t)ms_sleep(seconds * 1000);
}

// let process sleep
static inline uint32_t usleep(int32_t useconds)
{
    return (uint32_t)ms_sleep(useconds / 1000);
}

///////////////////////////////////////
// 2. File Management
//
// open() and creat() are in fcntl.h
//

// read [n] bytes from [fd] to [buffer]. Return bytes read or -1 on error.
extern ssize_t read(int fd, void *buffer, size_t n);

// writes [n] bytes from [buffer] to [fd]. Return bytes written or -1 on error.
extern ssize_t write(int fd, const void *buffer, size_t n);

// closes [fd]
extern int32_t close(int fd);

/// @brief Resize file to length bytes, discards data beyond length, fills new
/// space with zeros.
/// @param path file path, file must be write-able
/// @param length New file size in bytes.
/// @return 0 on success, -1 on failure; sets errno.
extern int32_t truncate(const char *path, off_t length);

/// @brief Resize file to length bytes, discards data beyond length, fills new
/// space with zeros.
/// @param fd file descriptor of file opened for writing
/// @param length New file size in bytes.
/// @return 0 on success, -1 on failure; sets errno.
extern int32_t ftruncate(int fd, off_t length);

// create a (hard)link [from] existing file [to] link
extern int32_t link(const char *from, const char *to);

// remove link [name]
extern int32_t unlink(const char *pathname);

// remove directory (must be empty)
extern int rmdir(const char *path);

// change working directory
extern int32_t chdir(const char *path);

// duplicate open file descriptor. Returns new file descriptor or -1 on error
extern int dup(int fd);

/// @brief Sets file offset.
/// @param fd file descriptor
/// @param offset offset relative to whence
/// @param whence SEEK_SET, SEEK_CUR, or SEEK_END
/// @return Offset from file beginning, -1 on error
extern off_t lseek(int fd, off_t offset, int whence);

///////////////////////////////////////
// 3. Device Management

///////////////////////////////////////
// 4. Information Management

// provides some system information at runtime. Implemented in the stdc lib
#define _SC_PAGE_SIZE 0
#define _SC_PAGESIZE _SC_PAGE_SIZE
#define _SC_ARG_MAX 1
#define _SC_OPEN_MAX 2
#define _SC_ATEXIT_MAX 3
extern long sysconf(int name);

///////////////////////////////////////
// 5. Communication

// create a (one-way) pipe. pipe_descriptors[0] for reading,
// pipe_descriptors[1] for writing.
extern int32_t pipe(int pipe_descriptors[2]);

///////////////////////////////////////
// 6. Protection

/// @brief Get (real) user ID of calling process.
/// @return Real user ID.
uid_t getuid();

/// @brief Get (real) group ID of calling process.
/// @return Real group ID.
uid_t getgid();

/// @brief Get effective user ID of calling process.
/// @return Effective user ID.
uid_t geteuid();

/// @brief Get effective group ID of calling process.
/// @return Effective group ID.
uid_t getegid();

/// @brief Get real, effective and saved user IDs of calling process.
/// @param ruid Real user ID will be stored here
/// @param euid Effective user ID will be stored here
/// @param suid Saved user ID will be stored here
/// @return 0 on success, -1 on failure. Sets errno.
extern int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);

/// @brief Get real, effective and saved group IDs of calling process.
/// @param rgid Real group ID will be stored here
/// @param egid Effective group ID will be stored here
/// @param sgid Saved group ID will be stored here
/// @return 0 on success, -1 on failure. Sets errno.
extern int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);

/// @brief Sets real, effective and saved user IDs of calling process.
/// If the caller is un-privileged, the new IDs must be equal to the real,
/// effective or saved user ID.
/// @param ruid New real user ID or -1 to leave unchanged.
/// @param euid New effective user ID or -1 to leave unchanged.
/// @param suid New saved user ID or -1 to leave unchanged.
/// @return 0 on success, -1 on failure. Sets errno.
extern int setresuid(uid_t ruid, uid_t euid, uid_t suid);

/// @brief Sets real, effective and saved group IDs of calling process.
/// If the caller is un-privileged, the new IDs must be equal to the real,
/// effective or saved group ID.
/// @param rgid New group group ID or -1 to leave unchanged.
/// @param egid New effective group ID or -1 to leave unchanged.
/// @param sgid New saved group ID or -1 to leave unchanged.
/// @return 0 on success, -1 on failure. Sets errno.
extern int setresgid(gid_t rgid, gid_t egid, gid_t sgid);

/// @brief
/// @param uid User ID to set.
/// @return 0 on success, -1 on failure. Sets errno.
extern int setuid(uid_t uid);

/// @brief
/// @param uid Group ID to set.
/// @return 0 on success, -1 on failure. Sets errno.
extern int setgid(gid_t gid);

/// @brief Sets the effective user ID of the calling process.
/// @param euid New effective user ID. If the caller is un-privileged, the
/// value must be equal to the real, effective or saved user ID.
/// @return 0 on success, -1 on failure. Sets errno.
static inline int seteuid(uid_t euid) { return setresuid(-1, euid, -1); }

/// @brief Sets the effective group ID of the calling process.
/// @param egid New effective group ID. If the caller is un-privileged, the
/// value must be equal to the real, effective or saved group ID.
/// @return 0 on success, -1 on failure. Sets errno.
static inline int setegid(gid_t egid) { return setresgid(-1, egid, -1); }

/// @brief Get additional group IDs of calling process.
/// @param size Size of list array.
/// @param list Destination array for group IDs.
/// @return Total number of groups if size is 0; otherwise, number of groups
/// copied to list. On failure, -1 is returned and errno is set.
extern int getgroups(int size, gid_t *list);

/// @brief Set additional group IDs of calling process. Will replace previously
/// set groups.
/// @param size Size of the list array.
/// @param list Array of group IDs to set.
/// @return 0 on success, -1 on failure. Sets errno.
extern int setgroups(size_t size, const gid_t *list);

/// @brief Change ownership of a file.
/// @param path Path to file.
/// @param owner New owner user ID or -1 to leave unchanged.
/// @param group New group ID or -1 to leave unchanged.
/// @return 0 on success, -1 on failure. Sets errno.
extern int chown(const char *path, uid_t owner, gid_t group);

/// @brief Change ownership of a file.
/// @param fd File descriptor of file.
/// @param owner New owner user ID or -1 to leave unchanged.
/// @param group New group ID or -1 to leave unchanged.
/// @return 0 on success, -1 on failure. Sets errno.
extern int fchown(int fd, uid_t owner, gid_t group);

///////////////////////////////////////

/// @brief Change program break size / heap size.
/// @param increment bytes by which the heap should grow or shrink
/// @return On success the previous program break; on failure ((void*) -1) and
/// errno will be set to ENOMEM
extern void *sbrk(intptr_t increment);

extern int32_t uptime();

int isatty(int fd);
