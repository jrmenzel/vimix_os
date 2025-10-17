/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/limits.h>

struct super_block;
struct inode;
struct file;
struct statvfs;

struct super_operations
{
    struct inode *(*iget_root)(struct super_block *sb);

    struct inode *(*alloc_inode)(struct super_block *sb, mode_t mode);

    int (*write_inode)(struct inode *ip);

    ssize_t (*statvfs)(struct super_block *sb, struct statvfs *to_fill);
};

/// @brief Get root inode of file system. Not locked.
#define VFS_SUPER_IGET_ROOT(sb) (sb)->s_op->iget_root((sb))
#define VFS_SUPER_ALLOC_INODE(sb, mode) (sb)->s_op->alloc_inode((sb), (mode))
#define VFS_SUPER_WRITE_INODE(ip) (ip)->i_sb->s_op->write_inode((ip))
#define VFS_SUPER_STATVFS(sb, buf) (sb)->s_op->statvfs((sb), (buf))

struct inode_operations
{
    struct inode *(*iops_create)(struct inode *iparent, char name[NAME_MAX],
                                 mode_t mode, int32_t flags, dev_t device);

    struct inode *(*iops_open)(struct inode *iparent, char name[NAME_MAX],
                               int32_t flags);

    void (*iops_read_in)(struct inode *ip);

    struct inode *(*iops_dup)(struct inode *ip);

    void (*iops_put)(struct inode *ip);

    struct inode *(*iops_dir_lookup)(struct inode *dir, const char *name,
                                     uint32_t *poff);

    int (*iops_dir_link)(struct inode *dir, char *name, ino_t inum);

    ssize_t (*iops_get_dirent)(struct inode *dir, size_t dir_entry_addr,
                               bool addr_is_userspace, ssize_t seek_pos);

    ssize_t (*iops_read)(struct inode *ip, bool addr_is_userspace, size_t dst,
                         size_t off, size_t n);

    ssize_t (*iops_link)(struct inode *dir, struct inode *ip,
                         char name[NAME_MAX]);

    ssize_t (*iops_unlink)(struct inode *dir, char name[NAME_MAX],
                           bool delete_files, bool delete_directories);

    ssize_t (*iops_truncate)(struct inode *ip, off_t length);
};

/// @brief Opens the inode inside of directory iparent with the given name
/// or creates one if none existed.
/// @param iparent Parent directory inode, unlocked
/// @param name File name
/// @param mode File mode
/// @param flags Create flags
/// @param device Device of the inode in case it's a device file
/// @return NULL on failure, requested inode otherwise.
#define VFS_INODE_CREATE(iparent, name, mode, flags, device)               \
    (iparent)->i_sb->i_op->iops_create((iparent), (name), (mode), (flags), \
                                       (device))

/// @brief Opens the inode inside of directory iparent with the given name.
/// @param iparent Parent directory inode, unlocked
/// @param name File name
/// @param flags Truncate inode content if O_TRUNC is set (regular files
/// only)
/// @return NULL on failure, requested inode otherwise.
#define VFS_INODE_OPEN(iparent, name, flags) \
    (iparent)->i_sb->i_op->iops_open((iparent), (name), (flags))

/// @brief Reads the inode metadata from disk, called during the first
/// inode_lock().
/// @param ip Inode with attribute valid == false.
/// @return Number of bytes written to ip or a negative value on error.
#define VFS_INODE_READ_IN(ip) (ip)->i_sb->i_op->iops_read_in((ip))

/// @brief Increment reference count for ip.
/// @return ip to enable ip = inode_dup(ip1) idiom.
#define VFS_INODE_DUP(ip) (ip)->i_sb->i_op->iops_dup((ip))

/// @brief Decreases ref count. If the inode was "deleted" (zero links) and this
/// was the last reference, delete on disk. Note that this can require a new log
/// begin/end.
/// @param ip Inode with held reference.
#define VFS_INODE_PUT(ip) (ip)->i_sb->i_op->iops_put((ip))

/// @brief Look for a directory entry in a directory.
/// Increases ref count (release with inode_put()).
/// @param dir Directory to look in, should be locked.
/// @param name Name of entry (e.g. file name)
/// @param poff Pointer to a returned offset inside of the dir. Can be NULL.
/// @return Inode of entry on success or NULL. Returned inode is NOT locked, and
/// has an increases ref count (release with inode_put()).
#define VFS_INODE_DIR_LOOKUP(dir, name, inum) \
    (dir)->i_sb->i_op->iops_dir_lookup((dir), (name), (inum))

/// @brief Write a new directory entry (name, inum) into the directory `dir`.
/// @param dir directory to edit
/// @param name file name of new entry
/// @param inum inode of new entry
/// @return 0 on success, -1 on failure (e.g. out of disk blocks).
#define VFS_INODE_DIR_LINK(dir, name, inum) \
    (dir)->i_sb->i_op->iops_dir_link((dir), (name), (inum))

/// @brief For the syscall to get directory entries get_dirent() from dirent.h.
/// @param dir Directory inode
/// @param dir_entry_addr address of buffer to fill with one struct dirent
/// @param addr_is_userspace True if dir_entry_addr is a user virtual address.
/// @param seek_pos A seek pos previously returned by inode_get_dirent or 0.
/// @return next seek_pos on success, 0 on dir end and -1 on error.
#define VFS_INODE_GET_DIRENT(dir, dir_entry_addr, addr_is_userspace, seek_pos) \
    (dir)->i_sb->i_op->iops_get_dirent((dir), (dir_entry_addr),                \
                                       (addr_is_userspace), (seek_pos))

/// @brief Read data from inode.
/// Caller must hold ip->lock.
/// @param ip Inode belonging to a file system
/// @param dst_addr_is_userspace If true, dst_addr is a user virtual address
/// (kernel addr otherwise)
/// @param dst_addr Destination address.
/// @param off Offset in file where to read from.
/// @param n Maximum number of bytes to read.
/// @return Number of bytes successfully read.
#define VFS_INODE_READ(ip, addr_is_userspace, dst, off, n) \
    (ip)->i_sb->i_op->iops_read((ip), (addr_is_userspace), (dst), (off), (n))
#define VFS_INODE_LINK(dir, ip, name) \
    (dir)->i_sb->i_op->iops_link((dir), (ip), (name))
#define VFS_INODE_UNLINK(dir, name, delete_files, delete_directories) \
    (dir)->i_sb->i_op->iops_unlink((dir), (name), (delete_files),     \
                                   (delete_directories))

/// @brief Resize inode (discard extra contents or create empty data).
#define VFS_INODE_TRUNCATE(ip, length) \
    (ip)->i_sb->i_op->iops_truncate((ip), (length))

struct file_operations
{
    ssize_t (*fops_write)(struct file *f, size_t addr, size_t n);
};

/// @brief Write n bytes from buffer at addr to file f.
/// @param f File to write to.
/// @param addr Source address of data to write.
/// @param n Number of bytes to write
/// @return Number of bytes successfully written.
#define VFS_FILE_WRITE(f, addr, n) \
    (f)->ip->i_sb->f_op->fops_write((f), (addr), (n))
