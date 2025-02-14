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

// read [n] bytes from [fd] to [buffer]. Return bytes read or -1 on error.
extern ssize_t read(int fd, void *buffer, size_t n);

// writes [n] bytes from [buffer] to [fd]. Return bytes written or -1 on error.
extern ssize_t write(int fd, const void *buffer, size_t n);

// open() and creat() are in fcntl.h

// closes [fd]
extern int32_t close(int fd);

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

///////////////////////////////////////

/// @brief Change program break size / heap size.
/// @param increment bytes by which the heap should grow or shrink
/// @return On success the previous program break; on failure ((void*) -1) and
/// errno will be set to ENOMEM
extern void *sbrk(intptr_t increment);

extern int32_t uptime();

int isatty(int fd);
