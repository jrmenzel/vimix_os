/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/param.h>
#include <kernel/spinlock.h>

#define PIPE_SIZE 512

/// @brief A pipe consists of this struct and two files (struct file)
/// which have a pointer to this pipe object.
struct pipe
{
    struct spinlock lock;
    char data[PIPE_SIZE];  ///< circular buffer
    size_t nread;          ///< number of bytes read
    size_t nwrite;         ///< number of bytes written
    bool read_open;        ///< read fd is still open
    bool write_open;       ///< write fd is still open
};
_Static_assert(sizeof(struct pipe) <= PAGE_SIZE, "struct pipe too big");

/// @brief Creates a pipe: two files and a struct pipe in the
/// background.
/// @param f0 read end
/// @param f1 write end
/// @return 0 on success
int32_t pipe_alloc(struct file **f0, struct file **f1);

/// @brief Close the pipe, called from the files belonging
/// to this pipe. After being called from both files it will
/// free the pipe *pipe.
/// @param pipe Pipe to close from either the reading or writing end.
/// @param close_writing_end If true close from the writing end.
void pipe_close(struct pipe *pipe, bool close_writing_end);

/// @brief Read up to n bytes from the pipe.
/// @param pipe Pipe to read from.
/// @param dst_va Destination address in user virtual address space.
/// @param n Max number of bytes to read.
/// @return Number of bytes read or -1 on error.
ssize_t pipe_read(struct pipe *pipe, size_t src_kernel_addr, size_t n);

/// @brief Write up to n bytes to a pipe.
/// @param pipe Pipe to write to.
/// @param src_va Source address of data in user virtual address space.
/// @param n Max number of bytes to write.
/// @return Number of bytes written or -1 on error.
ssize_t pipe_write(struct pipe *pipe, size_t src_user_addr, size_t n);
