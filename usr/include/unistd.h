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

// let process sleep
extern int32_t sleep(int32_t seconds);

///////////////////////////////////////
// 2. File Management

// read [n] bytes from [fd] to [buffer]. Return bytes read or -1 on error.
extern ssize_t read(int fd, void *buffer, size_t n);

// writes [n] bytes from [buffer] to [fd]. Return bytes written or -1 on error.
extern ssize_t write(int fd, const void *buffer, size_t n);

// open() is in fcntl.h

// closes [fd]
extern int32_t close(int fd);

// create a (hard)link [from] existing file [to] link
extern int32_t link(const char *from, const char *to);

// remove link [name]
extern int32_t unlink(const char *pathname);

// change working directory
extern int32_t chdir(const char *path);

// duplicate open file descriptor. Returns new file descriptor or -1 on error
extern int dup(int fd);

///////////////////////////////////////
// 3. Device Management

///////////////////////////////////////
// 4. Information Management

///////////////////////////////////////
// 5. Communication

// create a (one-way) pipe. pipe_descriptors[0] for reading,
// pipe_descriptors[1] for writing.
extern int32_t pipe(int pipe_descriptors[2]);

///////////////////////////////////////
// 6. Protection

///////////////////////////////////////

// change data segment size
extern void *sbrk(intptr_t increment);

extern int32_t uptime();
