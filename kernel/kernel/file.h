/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/dentry.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/kref.h>
#include <kernel/list.h>
#include <kernel/stat.h>

/// @brief Represents an open file. Each process has an array
/// of these. The "file descriptor" in C is simply the index into that array.
struct file
{
    struct list_head list;  ///< double linked list of all open files
    mode_t mode;  ///< file type and access rights, needed for pipes which don't
                  ///< have an inode
    int32_t flags;      ///< file create flags
    struct kref ref;    ///< reference count
    struct pipe *pipe;  ///< used if the file belongs to a pipe
    struct dentry *dp;  ///< dentry of the file
    uint32_t off;       ///< for files
};

/// @brief Common code to check file mode.
/// E.g. if no type is provided, regular file will be the default for multiple
/// syscalls.
/// @param mode File mode chich might get modified
/// @return True if the mode can be used, false on errors.
bool check_and_adjust_mode(mode_t *mode, mode_t default_type);

/// @brief Allocate a file structure. ONLY ref is initialized!
struct file *file_alloc();

struct file *file_alloc_init(mode_t mode, int32_t flags, struct dentry *dp);

/// @brief Open file at pathname with flags and mode.
/// @param pathname Path to file to open.
/// @param flags Open flags.
/// @param mode File mode.
/// @return File descriptor or -errno on error.
syserr_t do_open(char *pathname, int32_t flags, mode_t mode);

/// @brief Close file f. (Decrement ref count, close when reaches 0.)
void file_close(struct file *f);

/// @brief Increment ref count for file f.
/// @returns File parameter f.
struct file *file_get(struct file *f);

/// @brief Inits the global table of all open files. Call before
/// allocating any files.
void file_init();

/// @brief Read from file f.
/// @param f File to read from.
/// @param addr a user virtual address.
/// @param n Max bytes to read.
/// @return Number of bytes read or -1 on error.
syserr_t do_read(struct file *f, size_t addr, size_t n);

/// @brief Write to file.
/// @param f File to write to.
/// @param addr a user virtual address.
/// @param n Max bytes to write.
/// @return Number of bytes written or -1 on error.
syserr_t do_write(struct file *f, size_t addr, size_t n);

/// @brief Most of syscall lseek
/// @param f File of which to change read pointer
/// @param offset offset relative to a position
/// @param whence position, SEEK_SET etc.
/// @return 0 on success, -1 on error
syserr_t do_lseek(struct file *f, ssize_t offset, int whence);
