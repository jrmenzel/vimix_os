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
    uint nread;     ///< number of bytes read
    uint nwrite;    ///< number of bytes written
    int readopen;   ///< read fd is still open
    int writeopen;  ///< write fd is still open
};

int pipe_alloc(struct file**, struct file**);
void pipe_close(struct pipe*, int);
int pipe_read(struct pipe*, uint64, int);
int pipe_write(struct pipe*, uint64, int);
