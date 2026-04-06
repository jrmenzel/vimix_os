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

    struct inode *s_root;   ///< inode for root dir of mounted file sys
    struct dentry *d_root;  ///< dentry for root dir of mounted file sys
    void *s_fs_info;        ///< Filesystem private info

    struct inode
        *imounted_on;  ///< inode this FS is mounted on, owns a reference
    struct dentry
        *dmounted_on;  ///< dentry this FS is mounted on, owns a reference
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
    // super block, mode, IDs, device and inode number are considered to be
    // reasonably static to not require a lock for read access. These fields are
    // needed for path resolution and permission checks before acquiring the
    // inode lock.
    // Readers might see slightly outdated values if a concurrent call to
    // chmod/chown/chgrp/chmod happens. This is considered acceptable. All
    // writes are 32-bit and will be atomic, so the values will at least be
    // consistent.

    struct super_block *i_sb;  ///< info on FS this inode belongs to
    mode_t i_mode;             ///< type and access rights, see stat.h
    uid_t uid;                 ///< owner user id
    gid_t gid;                 ///< owner group id
    ino_t inum;                ///< Inode number

    /// Device number (NOT where the file is stored: ->i_sb->dev)
    /// e.g. mknod(..., dev) will result in dev being the dev
    /// parameter from mknod() and i_sb->dev being the device ID of the
    /// filesystem this file is located on.
    /// Identical to i_sb->dev for regular files (for char/block r/w)
    dev_t dev;

    /// @brief Reference count. If 0 it means that this entry in
    /// inode table is free. Access via inode_get()/inode_put().
    struct kref ref;

    /// @brief Exclusive write access holder.
    struct sleeplock write_exclusive_lock;

    /// @brief Protects everything below here. Aquire lock for read/write access
    /// for everything below and write access for everything that modifies the
    /// inode.
    /// To update the inode, write_exclusive_holder must be set to the current
    /// process PID. This allows processes to sleep while holding
    /// write_exclusive_holder.
    struct sleeplock lock;

    int16_t nlink;  ///< links to this inode
    uint32_t size;  ///< size of file (bytes)

    // note: real UNIX has also the files last access time atime, this is
    // not supported here.
    time_t ctime;  ///< time of last modification of file metadata
    time_t mtime;  ///< time of last modification of file content

    struct super_block *is_mounted_on;  ///< if set a file system is mounted on
                                        ///< this (dir) inode

    /// list of all inodes on the FS the inode belongs to.
    struct list_head fs_inode_list;
};

#define inode_from_list(ptr) container_of(ptr, struct inode, fs_inode_list)

void inode_init(struct inode *ip, struct super_block *sb, ino_t inum);

/// @brief De-initialize inode, does not free the memory.
/// @param ip The inode.
void inode_del(struct inode *ip);

/// @brief inits the filesystem with device dev becoming "/"
/// @param dev device of the root fs
void mount_root(dev_t dev, const char *fs_name);

/// @brief Lock the given inode.
/// Reads the inode from disk if necessary.
void inode_lock(struct inode *ip);

/// @brief Unlock the given inode.
void inode_unlock(struct inode *ip);

/// @brief Locks both inodes (in a deadlock free way).
/// @param ip0 First inode.
/// @param ip1 Second inode.
void inode_lock_2(struct inode *ip0, struct inode *ip1);

static inline void inode_unlock_2(struct inode *ip0, struct inode *ip1)
{
    inode_unlock(ip0);
    inode_unlock(ip1);
}

/// @brief Increase reference count for the inode.
/// @param ip The inode.
static inline struct inode *inode_get(struct inode *ip)
{
    kref_get(&ip->ref);
    return ip;
}

/// @brief Drop a reference to an in-memory inode.
/// If that was the last reference, the inode gets freed.
/// All calls to inode_put() must be inside a transaction in
/// case it has to free the inode.
static inline void inode_put(struct inode *ip) { VFS_INODE_PUT(ip); }

/// @brief Common idiom: unlock, then put.
void inode_unlock_put(struct inode *ip);

/// @brief Aquire exclusive write access to inode.
/// @param ip Unlocked inode with exclusive access set to PID of caller after
/// return.
void inode_lock_exclusive(struct inode *ip);

/// @brief Frees exlusive write access to inode.
/// @param ip Unlocked inode.
void inode_unlock_exclusive(struct inode *ip);

void inode_lock_exclusive_2(struct inode *ip1, struct inode *ip2);

static inline void inode_unlock_exclusive_2(struct inode *ip1,
                                            struct inode *ip2)
{
    inode_unlock_exclusive(ip1);
    inode_unlock_exclusive(ip2);
}

/// @brief Copy stat information from inode. Caller must hold ip->lock.
/// @param ip Source inode
/// @param st Target stat
void inode_stat(struct inode *ip, struct stat *st);

//
// directories
//

/// @brief Helper to compare two file names
/// @param s0 string 0
/// @param s1 string 1
/// @return 0 if the file names are equal
int file_name_cmp(const char *s, const char *t);

//
// debug code
//

void debug_print_inode(struct inode *ip);

void debug_print_inodes();
