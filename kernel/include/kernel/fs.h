/* SPDX-License-Identifier: MIT */
#pragma once

// On-disk file system format.
// Both the kernel and user programs use this header file.

#include <kernel/kernel.h>
#include <kernel/sleeplock.h>
#include <kernel/stat.h>
#include <kernel/xv6fs.h>

#define DEFAULT_ACCESS_MODES (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

/// in-memory copy of an inode
struct inode
{
    dev_t i_sb_dev;

    /// Device number (NOT where the file is stored: ->i_sb_dev)
    /// e.g. mknod(..., dev) will result in dev being the dev
    /// parameter from mknod() and i_sb_dev being the device ID of the
    /// filesystem this file is located on.
    /// Identical to i_sb_dev for regular files (for char/block r/w)
    dev_t dev;
    uint32_t inum;          ///< Inode number
    int ref;                ///< Reference count
    struct sleeplock lock;  ///< protects everything below here
    int valid;              ///< inode has been read from disk?

    mode_t i_mode;  ///< type and access rights, see stat.h
    int16_t nlink;
    uint32_t size;

    /// Inode content
    ///
    /// The content (data) associated with each inode is stored
    /// in blocks on the disk. The first NDIRECT block numbers
    /// are listed in ip->addrs[].  The next NINDIRECT blocks are
    /// listed in block ip->addrs[NDIRECT].
    uint32_t addrs[NDIRECT + 1];

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    char path[PATH_MAX];
#endif
};

/// @brief inits the filesystem with device dev becoming "/"
/// @param dev device of the root fs
void init_root_file_system(dev_t dev);

int inode_dir_link(struct inode *, char *, uint32_t);

/// @brief Look for a directory entry in a directory.
/// Increases ref count (release with inode_put()).
/// @param dir Directory to look in
/// @param name Name of entry (e.g. file name)
/// @param poff If found, set *poff to byte offset of entry.
/// @return Inode of entry on success or NULL.
struct inode *inode_dir_lookup(struct inode *dir, const char *name,
                               uint32_t *poff);

/// @brief Allocate an inode on device dev.
/// Mark it as allocated by giving it type based on mode.
/// @param dev The device/filesystem to allocate on.
/// @param mode Type and access rights of the new inode.
/// @return An unlocked but allocated and referenced inode,
/// or NULL if there is no free inode.
struct inode *inode_alloc(dev_t dev, mode_t mode);

/// @brief Find the inode with number inum on device dev.
/// Does not lock the inode and does not read it from disk.
/// Assumes valid input: you get an inode or a kernel panic.
/// @return in-memory copy of inode
struct inode *iget(dev_t dev, uint32_t inum);

/// @brief Opens the inode belonging to path or creates one if none existed.
/// Used for regular files and directories.
/// @param path Path / file name
/// @param mode File mode
/// @param device Device the inode is located
/// @return NULL on failure, requested inode otherwise.
struct inode *inode_open_or_create(const char *path, mode_t mode, dev_t device);

/// @brief Like inode_open_or_create() but only returns success codes.
/// @return -1 on failure, 0 otherwise.
ssize_t inode_open_or_create2(const char *path, mode_t mode, dev_t device);

/// @brief Increment reference count for ip.
/// @return ip to enable ip = inode_dup(ip1) idiom.
struct inode *inode_dup(struct inode *ip);

/// @brief called from main() once
void inode_init();

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

/// @brief Copy a modified in-memory inode to disk.
/// Must be called after every change to an ip->xxx field
/// that lives on disk.
/// Caller must hold ip->lock.
void inode_update(struct inode *ip);

int file_name_cmp(const char *, const char *);

/// @brief get inode based on the path.
/// Increases ref count (release with inode_put()).
struct inode *inode_from_path(const char *path);

/// @brief get inode of the parent directory
/// Increases ref count (release with inode_put()).
struct inode *inode_of_parent_from_path(const char *path, char *name);

/// @brief Read data from inode.
/// Caller must hold ip->lock.
/// @param ip Inode belonging to a file system
/// @param dst_addr_is_userspace If true, dst_addr is a user virtual address
/// (kernel addr otherwise)
/// @param dst_addr Destination address.
/// @param off
/// @param n Maximum number of bytes to read.
/// @return Number of bytes successfully read.
ssize_t inode_read(struct inode *ip, bool dst_addr_is_userspace,
                   size_t dst_addr, size_t off, size_t n);

/// @brief Copy stat information from inode. Caller must hold ip->lock.
/// @param ip Source inode
/// @param st Target stat
void inode_stat(struct inode *ip, struct stat *st);

/// @brief Write data to inode.
/// Caller must hold ip->lock.
/// Returns the number of bytes successfully written.
/// If the return value is less than the requested n,
/// there was an error of some kind.
/// @param ip Inode belonging to a file system
/// @param src_addr_is_userspace If true, src_addr  is a user virtual address
/// (kernel addr otherwise)
/// @param src_addr Source address.
/// @param off
/// @param n Number of bytes to write.
/// @return Number of bytes successfully written.
ssize_t inode_write(struct inode *ip, bool src_addr_is_userspace,
                    size_t src_addr, size_t off, size_t n);

/// @brief Truncate inode (discard contents).
/// Caller must hold ip->lock.
/// @param ip inode to truncate.
void inode_trunc(struct inode *ip);
