/* SPDX-License-Identifier: MIT */
#pragma once

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
ssize_t vimix_sops_statvfs(struct super_block *sb, struct statvfs *to_fill);

/// @brief Opens the inode inside of directory iparent with the given name or
/// creates one if none existed.
/// @param iparent Parent directory inode, unlocked
/// @param name File name
/// @param mode File mode
/// @param flags Create flags
/// @param device Device of the inode in case it's a device file
/// @return NULL on failure, requested inode otherwise.
struct inode *vimixfs_iops_create(struct inode *iparent, char name[NAME_MAX],
                                  mode_t mode, int32_t flags, dev_t device);

/// @brief Opens the inode inside of directory iparent with the given name.
/// @param iparent Parent directory inode, unlocked
/// @param name File name
/// @param flags Truncate inode content if O_TRUNC is set (regular files only)
/// @return NULL on failure, requested inode otherwise.
struct inode *vimixfs_iops_open(struct inode *iparent, char name[NAME_MAX],
                                int32_t flags);

/// @brief Reads the inode metadata from disk, called during the first
/// inode_lock().
/// @param ip Inode with attribute valid == false.
void vimixfs_iops_read_in(struct inode *ip);

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
struct inode *vimixfs_iops_dir_lookup(struct inode *dir, const char *name,
                                      uint32_t *poff);

/// @brief Write a new directory entry (name, inum) into the directory `dir`.
/// @param dir directory to edit
/// @param name file name of new entry
/// @param inum inode of new entry
/// @return 0 on success, -1 on failure (e.g. out of disk blocks).
int vimixfs_iops_dir_link(struct inode *dir, char *name, ino_t inum);

/// @brief For the syscall to get directory entries get_dirent() from dirent.h.
/// @param dir Directory inode
/// @param dir_entry_addr address of buffer to fill with one struct dirent
/// @param addr_is_userspace True if dir_entry_addr is a user virtual address.
/// @param seek_pos A seek pos previously returned by inode_get_dirent or 0.
/// @return next seek_pos on success, 0 on dir end and -1 on error.
ssize_t vimixfs_iops_get_dirent(struct inode *dir, size_t dir_entry_addr,
                                bool addr_is_userspace, ssize_t seek_pos);

/// @brief Read data from inode.
/// Caller must hold ip->lock.
/// @param ip Inode belonging to a file system
/// @param dst_addr_is_userspace If true, dst_addr is a user virtual address
/// (kernel addr otherwise)
/// @param dst_addr Destination address.
/// @param off Offset in file where to read from.
/// @param n Maximum number of bytes to read.
/// @return Number of bytes successfully read.
ssize_t vimixfs_iops_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                          size_t off, size_t n);

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
/// @return Number of bytes successfully written.
ssize_t vimixfs_write(struct inode *ip, bool src_addr_is_userspace, size_t src,
                      size_t off, size_t n);

ssize_t vimixfs_iops_link(struct inode *dir, struct inode *ip,
                          char name[NAME_MAX]);

ssize_t vimixfs_iops_unlink(struct inode *dir, char name[NAME_MAX],
                            bool delete_files, bool delete_directories);

struct file;

ssize_t vimixfs_fops_write(struct file *f, size_t addr, size_t n);
