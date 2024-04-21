/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct file
{
    enum
    {
        FD_NONE,
        FD_PIPE,
        FD_INODE,
        FD_DEVICE
    } type;
    int ref;  // reference count
    char readable;
    char writable;
    struct pipe* pipe;  ///< FD_PIPE
    struct inode* ip;   ///< FD_INODE and FD_DEVICE
    uint off;           ///< FD_INODE
    short major;        ///< FD_DEVICE
};

#define major(dev) ((dev) >> 16 & 0xFFFF)
#define minor(dev) ((dev)&0xFFFF)
#define mkdev(m, n) ((uint)((m) << 16 | (n)))

/// map major device number to device functions.
struct devsw
{
    int (*read)(int, uint64, int);
    int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

struct file* file_alloc();
void file_close(struct file*);
struct file* file_dup(struct file*);
void file_init();
int file_read(struct file*, uint64, int n);
int file_stat(struct file*, uint64 addr);
int file_write(struct file*, uint64, int n);
