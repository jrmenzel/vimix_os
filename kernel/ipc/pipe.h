/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>

#define PIPESIZE 512

struct pipe
{
    struct spinlock lock;
    char data[PIPESIZE];
    uint32_t nread;   ///< number of bytes read
    uint32_t nwrite;  ///< number of bytes written
    int readopen;     ///< read fd is still open
    int writeopen;    ///< write fd is still open
};

int pipe_alloc(struct file**, struct file**);
void pipe_close(struct pipe*, int);
int pipe_read(struct pipe*, uint64_t, int);
int pipe_write(struct pipe*, uint64_t, int);
