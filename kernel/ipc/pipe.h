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

int pipealloc(struct file**, struct file**);
void pipeclose(struct pipe*, int);
int piperead(struct pipe*, uint64, int);
int pipewrite(struct pipe*, uint64, int);
