/* SPDX-License-Identifier: MIT */
#pragma once

// On-disk file system format.
// Both the kernel and user programs use this header file.

#include <fs/vfs.h>
#include <kernel/container_of.h>
#include <kernel/dirent.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/kref.h>
#include <kernel/list.h>
#include <kernel/rwspinlock.h>
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
    struct kobject kobj;  ///< for /sys/fs
    dev_t dev;            ///< device this super block belongs to

    struct file_system_type *s_type;  ///< File system type of device
    struct super_operations *s_op;    ///< FS specific super block operations
    struct inode_operations *i_op;    ///< FS specific inode operations
    struct file_operations *f_op;     ///< FS specific file operations

    struct inode *s_root;  ///< inode for root dir of mounted file sys
    void *s_fs_info;       ///< Filesystem private info

    struct inode
        *imounted_on;  ///< inode this FS is mounted on, owns a reference
    unsigned long s_mountflags;

    struct list_head fs_inode_list;  ///< list of all inodes on this FS
    struct rwspinlock fs_inode_list_lock;
};

#define super_block_from_kobj(ptr) container_of(ptr, struct super_block, kobj)

/// @brief Returns a new initialized super_block for mounting.
///        Indirectly protected by g_mount_lock.
struct super_block *sb_alloc_init();

/// @brief Frees a super_block during unmounting.
///        Indirectly protected by g_mount_lock.
/// @param sb Super block of unmounted FS to free.
void sb_free(struct super_block *sb);

/// in-memory copy of an inode
struct inode
{
    struct super_block *i_sb;  ///< info on FS this inode belongs to

    /// Device number (NOT where the file is stored: ->i_sb->dev)
    /// e.g. mknod(..., dev) will result in dev being the dev
    /// parameter from mknod() and i_sb->dev being the device ID of the
    /// filesystem this file is located on.
    /// Identical to i_sb->dev for regular files (for char/block r/w)
    dev_t dev;
    ino_t inum;       ///< Inode number
    struct kref ref;  ///< Reference count. If 0 it means that this entry in
                      ///< inode table is free.
    struct sleeplock lock;  ///< protects everything below here
    int32_t valid;  ///< inode has been read from disk? mode, size etc. are
                    ///< invalid if false

    mode_t i_mode;  ///< type and access rights, see stat.h
    int16_t nlink;  ///< links to this inode
    uint32_t size;

    uid_t uid;     ///< owner user id
    gid_t gid;     ///< owner group id
    time_t ctime;  ///< inode creation time
    time_t mtime;  ///< time of last modification of file content

    struct super_block *is_mounted_on;  ///< if set a file system is mounted on
                                        ///< this (dir) inode

    ///< list of all inodes on the FS the inode belongs to.
    struct list_head fs_inode_list;

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    char path[PATH_MAX];
#endif
};

#define inode_from_list(ptr) container_of(ptr, struct inode, fs_inode_list)

void inode_init(struct inode *ip, struct super_block *sb, ino_t inum);

/// @brief De-initialize inode, does not free the memory.
/// @param ip The inode.
void inode_del(struct inode *ip);

/// @brief inits the filesystem with device dev becoming "/"
/// @param dev device of the root fs
void mount_root(dev_t dev, const char *fs_name);

/// @brief Wrapper for _iops_open() which only returns success codes.
///        Used by mkdir() and mknod().
/// @return -ERRNO on failure, 0 otherwise.
ssize_t inode_create(const char *path, mode_t mode, dev_t device);

/// @brief Lock the given inode.
/// Reads the inode from disk if necessary.
void inode_lock(struct inode *ip);

/// @brief Locks both inodes (in a deadlock free way).
/// @param ip0 First inode.
/// @param ip1 Second inode.
void inode_lock_two(struct inode *ip0, struct inode *ip1);

/// @brief Increase reference count for the inode.
/// @param ip The inode.
static inline void inode_get(struct inode *ip) { kref_get(&ip->ref); }

/// @brief Drop a reference to an in-memory inode.
/// If that was the last reference, the inode gets freed.
/// All calls to inode_put() must be inside a transaction in
/// case it has to free the inode.
static inline void inode_put(struct inode *ip) { VFS_INODE_PUT(ip); }

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
int inode_dir_link(struct inode *dir, char *name, ino_t inum);

//
// debug code
//

void debug_print_inode(struct inode *ip);

void debug_print_inodes();
