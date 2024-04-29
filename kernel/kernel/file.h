/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Represents an open file. Each process has an array
/// of these. The "file descriptor" in C is simply the index into that array.
struct file
{
    mode_t mode;        ///< file type and access rights
    int32_t flags;      ///< file create flags
    int32_t ref;        ///< reference count
    struct pipe *pipe;  ///< used if the file belongs to a pipe
    struct inode *ip;   ///< for files, dirs, char and block devices
    uint32_t off;       ///< for files
    short major;        ///< FD_DEVICE
};

/// map major device number to device functions.
struct devsw
{
    ssize_t (*read)(bool, size_t, size_t);
    ssize_t (*write)(bool, size_t, size_t);
};

extern struct devsw devsw[];

#define CONSOLE 1

/// @brief Common code to check file mode.
/// E.g. if no type is provided, regular file will be the default for multiple
/// syscalls.
/// @param mode File mode chich might get modified
/// @return True if the mode can be used, false on errors.
bool check_and_adjust_mode(mode_t *mode, mode_t default_type);

/// @brief Allocate a file structure. ONLY ref is initialized!
struct file *file_alloc();

FILE_DESCRIPTOR file_open_or_create(char *pathname, int32_t flags, mode_t mode);

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

/// @brief creates a new hard link for file path_from with
/// new name path_to.
/// @return 0 on success, -1 on error
ssize_t file_link(char *path_from, char *path_to);

/// @brief Most of the syscall unlink
/// @param path path name
/// @return 0 on success, -1 on error
ssize_t file_unlink(char *path);
