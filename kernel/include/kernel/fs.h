/* SPDX-License-Identifier: MIT */
#pragma once

// On-disk file system format.
// Both the kernel and user programs use this header file.

#include <kernel/dirent.h>
#include <kernel/kernel.h>
#include <kernel/sleeplock.h>
#include <kernel/stat.h>

extern dev_t ROOT_DEVICE_NUMBER;
extern struct super_block *ROOT_SUPER_BLOCK;

#define DEFAULT_ACCESS_MODES (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

struct file_system_type;
struct inode;

/// @brief Generic Super Block binding together a device
/// and a file system type.
struct super_block
{
    dev_t dev;  ///< dev_t = INVALID_DEVICE means super block is unused

    struct file_system_type *s_type;

    struct inode *s_root;  ///< inode for root dir of mounted file sys
    void *s_fs_info;       ///< Filesystem private info

    struct inode
        *imounted_on;  ///< inode this FS is mounted on, owns a reference
    unsigned long s_mountflags;
};

/// in-memory copy of an inode
struct inode
{
    struct super_block *i_sb;

    /// Device number (NOT where the file is stored: ->i_sb->dev)
    /// e.g. mknod(..., dev) will result in dev being the dev
    /// parameter from mknod() and i_sb->dev being the device ID of the
    /// filesystem this file is located on.
    /// Identical to i_sb->dev for regular files (for char/block r/w)
    dev_t dev;
    uint32_t inum;  ///< Inode number
    int32_t ref;    ///< Reference count. If 0 it means that this entry in inode
                  ///< table is free. Lock xv6fs_itable.lock protects writes to
                  ///< this entry!
    struct sleeplock lock;  ///< protects everything below here
    int32_t valid;  ///< inode has been read from disk? mode, size etc. are
                    ///< invalid if false

    mode_t i_mode;  ///< type and access rights, see stat.h
    int16_t nlink;
    uint32_t size;

    struct super_block *is_mounted_on;  ///< if set a file system is mounted on
                                        ///< this (dir) inode

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    char path[PATH_MAX];
#endif
};

/// @brief inits the filesystem with device dev becoming "/"
/// @param dev device of the root fs
void mount_root(dev_t dev, const char *fs_name);

/// @brief Allocate an inode on device dev.
/// Mark it as allocated by giving it type based on mode.
/// @param dev The device/filesystem to allocate on.
/// @param mode Type and access rights of the new inode.
/// @return An unlocked but allocated and referenced inode,
/// or NULL if there is no free inode.
// struct inode *inode_alloc(dev_t dev, mode_t mode);

/// @brief Like xv6fs_iops_open_create() but only returns success codes.
/// @return -1 on failure, 0 otherwise.
ssize_t inode_open_or_create2(const char *path, mode_t mode, dev_t device);

/// @brief Lock the given inode.
/// Reads the inode from disk if necessary.
void inode_lock(struct inode *ip);

/// @brief Drop a reference to an in-memory inode.
/// If that was the last reference, the inode table entry can
/// be recycled.
/// If that was the last reference and the inode has no links
/// to it, free the inode (and its content) on disk.
/// All calls to inode_put() must be inside a transaction in
/// case it has to free the inode.
void inode_put(struct inode *ip);

/// @brief Unlock the given inode.
void inode_unlock(struct inode *ip);

/// @brief Common idiom: unlock, then put.
void inode_unlock_put(struct inode *ip);

/// @brief Read data from inode.
/// Caller must hold ip->lock.
/// @param ip Inode belonging to a file system
/// @param dst_addr_is_userspace If true, dst_addr is a user virtual address
/// (kernel addr otherwise)
/// @param dst_addr Destination address.
/// @param off Offset in file where to read from.
/// @param n Maximum number of bytes to read.
/// @return Number of bytes successfully read.
ssize_t inode_read(struct inode *ip, bool dst_addr_is_userspace,
                   size_t dst_addr, size_t off, size_t n);

/// @brief Copy stat information from inode. Caller must hold ip->lock.
/// @param ip Source inode
/// @param st Target stat
void inode_stat(struct inode *ip, struct stat *st);

//
// inode look ups
//

/// @brief get inode based on the path.
/// Shortly locks every inode on the path, so don't hold any inode locks when
/// calling to avoid dead-locks!
/// @param path Absolute or CWD relative path.
/// @return NULL on failure. Returned inode has an increased ref
/// count (release with inode_put()). (NOT locked)
struct inode *inode_from_path(const char *path);

/// @brief get inode of the parent directory
/// Shortly locks every inode on the path, so don't hold any inode locks when
/// calling to avoid dead-locks!
/// @param path Absolute or CWD relative path.
/// @param name Copy out the name component of path. Must have room for NAME_MAX
/// bytes.
/// @return NULL on failure. Returned inode has an increased ref
/// count (release with inode_put()). (NOT locked)
struct inode *inode_of_parent_from_path(const char *path, char *name);

/// @brief Look for a directory entry in a directory.
/// Increases ref count (release with inode_put()).
/// @param dir Directory to look in, should be locked.
/// @param name Name of entry (e.g. file name)
/// @return Inode of entry on success or NULL. Returned inode is NOT locked, and
/// has an increases ref count (release with inode_put()).
struct inode *inode_dir_lookup(struct inode *dir, const char *name);

//
// directories
//

/// @brief Helper to compare two file names
/// @param s0 string 0
/// @param s1 string 1
/// @return 0 if the file names are equal
int file_name_cmp(const char *s, const char *t);

/// @brief Write a new directory entry (name, inum) into the directory
/// `directory`.
/// @param dir directory to edit
/// @param name file name of new entry
/// @param inum inode of new entry
/// @return 0 on success, -1 on failure (e.g. out of disk blocks).
int inode_dir_link(struct inode *dir, char *name, uint32_t inum);

//
// debug code
//

void debug_print_inode(struct inode *ip);

void debug_print_inodes();
