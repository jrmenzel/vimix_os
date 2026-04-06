/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/dentry.h>
#include <fs/vimixfs/log.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/vimixfs.h>

extern const char *VIMIXFS_FS_NAME;

struct vimixfs_sb_private
{
    struct vimixfs_superblock sb;
    struct log log;
};

struct vimixfs_inode
{
    struct inode ino;

    /// Inode content
    ///
    /// The content (data) associated with each inode is stored
    /// in blocks on the disk. The first VIMIXFS_N_DIRECT_BLOCKS block numbers
    /// are listed in ip->addrs[].  The next VIMIXFS_N_INDIRECT_BLOCKS blocks
    /// are listed in block ip->addrs[VIMIXFS_N_DIRECT_BLOCKS].
    /// The last address points to a double indirect block.
    uint32_t addrs[VIMIXFS_N_DIRECT_BLOCKS + 2];
};

#define vimixfs_inode_from_inode(ptr) \
    container_of(ptr, struct vimixfs_inode, ino)

/// @brief Call before mounting.
void vimixfs_init();

struct inode *vimixfs_sops_alloc_inode(struct super_block *sb, mode_t mode);

/// @brief Copy a modified in-memory inode to disk.
/// Must be called after every change to an ip->xxx field
/// that lives on disk.
/// Caller must hold ip->lock.
int vimixfs_sops_write_inode(struct inode *ip);

/// @brief Exposes file system statistics.
/// @param sb Super block of VIMIX FS file system.
/// @param to_fill Pointer to an allocated buffer to fill.
/// @return 0 on success, -ERRNO on failure.
syserr_t vimix_sops_statvfs(struct super_block *sb, struct statvfs *to_fill);

/// @brief Opens the inode inside of directory iparent with the given name or
/// creates one if none existed.
/// @param parent Parent directory inode, unlocked
/// @param dp File dentry to create
/// @param mode File mode
/// @param flags Create flags
/// @return errno
syserr_t vimixfs_iops_create(struct inode *parent, struct dentry *dp,
                             mode_t mode, int32_t flags);

syserr_t vimixfs_iops_mknod(struct inode *parent, struct dentry *dp,
                            mode_t mode, dev_t dev);

syserr_t vimixfs_iops_mkdir(struct inode *parent, struct dentry *dp,
                            mode_t mode);

syserr_t vimixfs_fops_open(struct inode *ip, struct file *f);

/// @brief Reads the inode metadata from disk, called when creating the inode
/// in memory.
/// @param ip Inode to read metadata for.
void vimixfs_read_inode_metadata(struct inode *ip);

/// @brief Find the inode with number inum on super_block sb.
/// Does not lock the inode and does not read it from disk.
/// Assumes valid input: you get an inode or a kernel panic.
/// @param sb The filesystem to look on.
/// @param inum Inode number.
/// @return in-memory copy of inode, (NOT locked)
struct inode *vimixfs_iget(struct super_block *sb, ino_t inum);

/// @brief Returns the root inode of the file system.
static inline struct inode *vimixfs_sops_iget_root(struct super_block *sb)
{
    return vimixfs_iget(sb, VIMIXFS_ROOT_INODE);
}

/// @brief Decreases ref count. If the inode was "deleted" (zero links) and this
/// was the last reference, delete on disk. Note that this can require a new log
/// begin/end.
/// @param ip Inode with held reference.
void vimixfs_iops_put(struct inode *ip);

/// @brief Look for a directory entry in a directory.
/// Increases ref count (release with inode_put()).
/// @param dir Directory to look in, should be locked.
/// @param name Name of entry (e.g. file name)
/// @param poff Pointer to a returned offset inside of the dir. Can be NULL.
/// @return Inode of entry on success or NULL. Returned inode is NOT locked, and
/// has an increases ref count (release with inode_put()).
struct inode *vimixfs_lookup(struct inode *dir, const char *name,
                             uint32_t *poff);

struct dentry *vimixfs_iops_lookup(struct inode *parent, struct dentry *dp);

/// @brief Write a new directory entry (name, inum) into the directory `dir`.
/// @param dir directory to edit
/// @param name file name of new entry
/// @param inum inode of new entry
/// @return 0 on success, -errno on failure (e.g. out of disk blocks).
syserr_t vimixfs_dir_link(struct inode *dir, const char *name, ino_t inum);

/// @brief Like vimixfs_dir_link but does not check for existing entries with
/// the same name. Caller must have checked while holding the appropriate locks.
/// @param dir directory to edit
/// @param name file name of new entry
/// @param inum inode of new entry
/// @return 0 on success, -errno on failure.
syserr_t vimixfs_dir_link_unchecked(struct inode *dir, const char *name,
                                    ino_t inum);

/// @brief For the syscall to get directory entries get_dirent() from dirent.h.
/// @param dir Directory inode
/// @param dir_entry address of buffer to fill with one struct dirent
/// @param seek_pos A seek pos previously returned by inode_get_dirent or 0.
/// @return next seek_pos on success, 0 on dir end and -1 on error.
syserr_t vimixfs_iops_get_dirent(struct inode *dir, struct dirent *dir_entry,
                                 ssize_t seek_pos);

/// @brief Read data from inode.
/// Caller must hold ip->lock.
/// @param ip Inode belonging to a file system.
/// @param off Offset in file where to read from.
/// @param dst_addr Destination address.
/// @param n Maximum number of bytes to read.
/// @param dst_addr_is_userspace If true, dst_addr is a user virtual address
/// (kernel addr otherwise)
/// @return Number of bytes successfully read.
syserr_t vimixfs_iops_read(struct inode *ip, size_t off, size_t dst, size_t n,
                           bool addr_is_userspace);

/// @brief Write data to inode.
/// Caller must hold ip->lock.
/// Returns the number of bytes successfully written.
/// If the return value is less than the requested n,
/// there was an error of some kind.
/// @param ip Inode belonging to a file system
/// @param src_addr_is_userspace If true, src_addr  is a user virtual address
/// (kernel addr otherwise)
/// @param src_addr Source address.
/// @param off Offset in file.
/// @param n Number of bytes to write.
/// @return Number of bytes successfully written or negative error code.
syserr_t vimixfs_write(struct inode *ip, bool src_addr_is_userspace, size_t src,
                       size_t off, size_t n);

syserr_t vimixfs_iops_link(struct dentry *file_from, struct inode *dir_to,
                           struct dentry *new_link);

syserr_t vimixfs_iops_unlink(struct inode *parent, struct dentry *dp);

syserr_t vimixfs_iops_rmdir(struct inode *parent, struct dentry *dp);

syserr_t vimixfs_iops_truncate(struct dentry *dp, off_t new_size);

struct file;

syserr_t vimixfs_fops_write(struct file *f, size_t addr, size_t n);

syserr_t vimixfs_iops_chmod(struct dentry *dp, mode_t mode);

syserr_t vimixfs_iops_chown(struct dentry *dp, uid_t uid, gid_t gid);
