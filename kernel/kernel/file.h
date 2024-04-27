/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Represents an open file. Each process has an array
/// of these. The "file descriptor" in C is simply the index into that array.
struct file
{
    enum
    {
        FD_NONE,
        FD_PIPE,
        FD_INODE,
        FD_DEVICE
    } type;
    int ref;  ///< reference count
    char readable;
    char writable;
    struct pipe *pipe;  ///< FD_PIPE
    struct inode *ip;   ///< FD_INODE and FD_DEVICE
    uint32_t off;       ///< FD_INODE
    short major;        ///< FD_DEVICE
};

#define major(dev) ((dev) >> 16 & 0xFFFF)
#define minor(dev) ((dev)&0xFFFF)
#define mkdev(m, n) ((uint32_t)((m) << 16 | (n)))

/// map major device number to device functions.
struct devsw
{
    ssize_t (*read)(bool, size_t, size_t);
    ssize_t (*write)(bool, size_t, size_t);
};

extern struct devsw devsw[];

#define CONSOLE 1

/// @brief Allocate a file structure. ONLY ref is initialized!
struct file *file_alloc();

/// @brief Close file f. (Decrement ref count, close when reaches 0.)
void file_close(struct file *f);

/// @brief Increment ref count for file f.
/// @returns File parameter f.
struct file *file_dup(struct file *f);

/// @brief Inits the global table of all open files. Call before
/// allocating any files.
void file_init();

/// @brief Read from file f.
/// @param f File to read from.
/// @param addr a user virtual address.
/// @param n Max bytes to read.
/// @return Number of bytes read or -1 on error.
ssize_t file_read(struct file *f, size_t addr, size_t n);

/// @brief Get metadata about file.
/// @param f file to get stats from.
/// @param addr a user virtual address, pointing to a struct stat.
/// @return 0 on success, -1 on error
int32_t file_stat(struct file *f, size_t addr);

/// @brief Write to file.
/// @param f File to write to.
/// @param addr a user virtual address.
/// @param n Max bytes to write.
/// @return Number of bytes written or -1 on error.
ssize_t file_write(struct file *f, size_t addr, size_t n);
