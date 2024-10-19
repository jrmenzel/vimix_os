/* SPDX-License-Identifier: MIT */
#pragma once

// On-disk file system format.
// Both the kernel and user programs use this header file.

#include <kernel/dirent.h>
#include <kernel/kernel.h>
#include <kernel/sleeplock.h>
#include <kernel/stat.h>
#include <kernel/xv6fs.h>

extern dev_t ROOT_DEVICE_NUMBER;

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
    /// in blocks on the disk. The first XV6FS_N_DIRECT_BLOCKS block numbers
    /// are listed in ip->addrs[].  The next XV6FS_N_INDIRECT_BLOCKS blocks are
    /// listed in block ip->addrs[XV6FS_N_DIRECT_BLOCKS].
    uint32_t addrs[XV6FS_N_DIRECT_BLOCKS + 1];

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    char path[PATH_MAX];
#endif
};

/// @brief inits the filesystem with device dev becoming "/"
/// @param dev device of the root fs
void init_root_file_system(dev_t dev);

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

/// @brief Copy stat information from inode. Caller must hold ip->lock.
/// @param ip Source inode
/// @param st Target stat
void inode_stat(struct inode *ip, struct stat *st);

/// @brief Truncate inode (discard contents).
/// Caller must hold ip->lock.
/// @param ip inode to truncate.
void inode_trunc(struct inode *ip);

//
// inode look ups
//

/// @brief get inode based on the path.
/// Increases ref count (release with inode_put()).
/// Returned inode is locked
struct inode *inode_from_path(const char *path);

/// @brief get inode of the parent directory
/// Increases ref count (release with inode_put()).
/// Returned inode is locked
struct inode *inode_of_parent_from_path(const char *path, char *name);

/// @brief Look for a directory entry in a directory.
/// Increases ref count (release with inode_put()).
/// @param dir Directory to look in
/// @param name Name of entry (e.g. file name)
/// @param poff If found, set *poff to byte offset of entry.
/// @return Inode of entry on success or NULL.
struct inode *inode_dir_lookup(struct inode *dir, const char *name,
                               uint32_t *poff);

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

/// @brief For the syscall to get directory entries get_dirent() from dirent.h.
/// @param dir Directory inode
/// @param dir_entry_addr address of buffer to fill with one struct dirent
/// @param addr_is_userspace True if dir_entry_addr is a user virtual address.
/// @param seek_pos A seek pos previously returned by inode_get_dirent or 0.
/// @return next seek_pos on success, 0 on dir end and -1 on error.
ssize_t inode_get_dirent(struct inode *dir, size_t dir_entry_addr,
                         bool addr_is_userspace, ssize_t seek_pos);
