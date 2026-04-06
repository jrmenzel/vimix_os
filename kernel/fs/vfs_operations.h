/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/limits.h>

struct super_block;
struct inode;
struct file;
struct statvfs;
struct dentry;
struct dirent;

struct super_operations
{
    struct inode *(*iget_root)(struct super_block *sb);

    struct inode *(*alloc_inode)(struct super_block *sb, mode_t mode);

    int (*write_inode)(struct inode *ip);

    syserr_t (*statvfs)(struct super_block *sb, struct statvfs *to_fill);
};

/// @brief Get root inode of file system. Not locked.
#define VFS_SUPER_IGET_ROOT(sb) (sb)->s_op->iget_root((sb))
#define VFS_SUPER_ALLOC_INODE(sb, mode) (sb)->s_op->alloc_inode((sb), (mode))
#define VFS_SUPER_WRITE_INODE(ip) (ip)->i_sb->s_op->write_inode((ip))
#define VFS_SUPER_STATVFS(sb, buf) (sb)->s_op->statvfs((sb), (buf))

struct inode_operations
{
    syserr_t (*iops_create)(struct inode *parent, struct dentry *dp,
                            mode_t mode, int32_t flags);

    syserr_t (*iops_mknod)(struct inode *parent, struct dentry *dp, mode_t mode,
                           dev_t dev);

    syserr_t (*iops_mkdir)(struct inode *parent, struct dentry *dp,
                           mode_t mode);

    void (*iops_put)(struct inode *ip);

    struct dentry *(*iops_lookup)(struct inode *parent, struct dentry *dp);

    syserr_t (*iops_get_dirent)(struct inode *dir, struct dirent *dir_entry,
                                ssize_t seek_pos);

    // to file ops?
    syserr_t (*iops_read)(struct inode *ip, size_t off, size_t dst, size_t n,
                          bool addr_is_userspace);

    syserr_t (*iops_link)(struct dentry *file_from, struct inode *dir_to,
                          struct dentry *new_link);

    syserr_t (*iops_unlink)(struct inode *parent, struct dentry *dp);

    syserr_t (*iops_rmdir)(struct inode *parent, struct dentry *dp);

    syserr_t (*iops_truncate)(struct dentry *dp, off_t length);

    syserr_t (*iops_chmod)(struct dentry *dp, mode_t mode);

    syserr_t (*iops_chown)(struct dentry *dp, uid_t uid, gid_t gid);
};

/// @brief Opens the inode inside of directory iparent with the given name
/// or creates one if none existed.
/// @param parent Parent directory inode, unlocked
/// @param dp File dentry to create
/// @param mode File mode
/// @param flags Create flags
/// @return errno
#define VFS_INODE_CREATE(iparent, dp, mode, flags) \
    (iparent)->i_sb->i_op->iops_create((iparent), (dp), (mode), (flags))

#define VFS_INODE_MKNOD(parent, dp, mode, device) \
    (parent)->i_sb->i_op->iops_mknod((parent), (dp), (mode), (device))

#define VFS_INODE_MKDIR(parent, dp, mode) \
    (parent)->i_sb->i_op->iops_mkdir((parent), (dp), (mode))

/// @brief Decreases ref count. If the inode was "deleted" (zero links) and this
/// was the last reference, delete on disk. Note that this can require a new log
/// begin/end.
/// @param ip Inode with held reference.
#define VFS_INODE_PUT(ip) (ip)->i_sb->i_op->iops_put((ip))

/// @brief Look for a directory entry in a directory.
/// Increases ref count.
/// @param parent Directory dentry.
/// @param dp Dentry with name to look for.
/// @return dentry with ip set to found inode or NULL if not found.
#define VFS_INODE_LOOKUP(parent, dp) \
    (parent)->i_sb->i_op->iops_lookup((parent), (dp))

/// @brief For the syscall to get directory entries get_dirent() from dirent.h.
/// @param dir Directory inode
/// @param dir_entry address of buffer to fill with one struct dirent
/// @param seek_pos A seek pos previously returned by inode_get_dirent or 0.
/// @return next seek_pos on success, 0 on dir end and -1 on error.
#define VFS_INODE_GET_DIRENT(dir, dir_entry, seek_pos) \
    (dir)->i_sb->i_op->iops_get_dirent((dir), (dir_entry), (seek_pos))

/// @brief Read data from inode.
/// Caller must hold ip->lock.
/// @param ip Inode belonging to a file system
/// @param dst_addr_is_userspace If true, dst_addr is a user virtual address
/// (kernel addr otherwise)
/// @param dst_addr Destination address.
/// @param off Offset in file where to read from.
/// @param n Maximum number of bytes to read.
/// @return Number of bytes successfully read.
// #define VFS_INODE_READ(ip, addr_is_userspace, dst, off, n)
//     (ip)->i_sb->i_op->iops_read((ip), (addr_is_userspace), (dst), (off), (n))

#define VFS_INODE_READ_KERNEL(ip, off, dst, n) \
    (ip)->i_sb->i_op->iops_read((ip), (off), (dst), (n), (false))

#define VFS_INODE_LINK(file_from, dir_to, new_link) \
    (dir_to)->i_sb->i_op->iops_link((file_from), (dir_to), (new_link))

#define VFS_INODE_UNLINK(parent, dp) \
    (parent)->i_sb->i_op->iops_unlink((parent), (dp))

#define VFS_INODE_RMDIR(parent, dp) \
    (parent)->i_sb->i_op->iops_rmdir((parent), (dp))

/// @brief Resize inode (discard extra contents or create empty data).
#define VFS_INODE_TRUNCATE(dp, length) \
    (dp)->ip->i_sb->i_op->iops_truncate((dp), (length))

/// @brief Change mode of a file.
#define VFS_INODE_CHMOD(dp, mode) (dp)->ip->i_sb->i_op->iops_chmod((dp), (mode))

/// @brief Change owner and group of a file.
#define VFS_INODE_CHOWN(dp, uid, gid) \
    (dp)->ip->i_sb->i_op->iops_chown((dp), (uid), (gid))
struct file_operations
{
    syserr_t (*fops_open)(struct inode *ip, struct file *f);
    syserr_t (*fops_write)(struct file *f, size_t addr, size_t n);
};

#define VFS_FILE_OPEN(ip, f) (ip)->i_sb->f_op->fops_open((ip), (f))

/// @brief Write n bytes from buffer at addr to file f.
/// @param f File to write to.
/// @param addr Source address of data to write.
/// @param n Number of bytes to write
/// @return Number of bytes successfully written.
#define VFS_FILE_WRITE(f, addr, n) \
    (f)->dp->ip->i_sb->f_op->fops_write((f), (addr), (n))

#define VFS_FILE_READ(file, dst, n)                                          \
    (file)->dp->ip->i_sb->i_op->iops_read((f->dp->ip), (f->off), (dst), (n), \
                                          (true))
