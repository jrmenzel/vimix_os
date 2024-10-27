/* SPDX-License-Identifier: MIT */

#include <fs/vfs.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/mount.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <lib/minmax.h>

dev_t ROOT_DEVICE_NUMBER;
struct super_block *ROOT_SUPER_BLOCK = NULL;

struct super_block g_active_file_systems[MAX_MOUNTED_FILE_SYSTEMS] = {0};

struct super_block *get_free_super_block()
{
    for (size_t i = 0; i < MAX_MOUNTED_FILE_SYSTEMS; ++i)
    {
        if (g_active_file_systems[i].dev == INVALID_DEVICE)
        {
            return &g_active_file_systems[i];
        }
    }
    return NULL;
}

void free_super_block(struct super_block *sb)
{
    for (size_t i = 0; i < MAX_MOUNTED_FILE_SYSTEMS; ++i)
    {
        if (&g_active_file_systems[i] == sb)
        {
            // call destructor
            // g_active_file_systems[i].s_type->kill_sb(sb);
            // mark entry as free
            inode_put(sb->imounted_on);
            inode_put(sb->s_root);
            sb->imounted_on = NULL;
            sb->dev = INVALID_DEVICE;
        }
    }
}

ssize_t mount(const char *source, const char *target,
              const char *filesystemtype, unsigned long mountflags,
              size_t addr_data)
{
    struct file_system_type **file_system =
        find_filesystem(filesystemtype, strlen(filesystemtype));

    if (*file_system == NULL)
    {
        return -EINVAL;
    }

    struct inode *i_src = inode_from_path(source);
    if (i_src == NULL)
    {
        return -ENODEV;
    }

    inode_lock(i_src);
    if (!S_ISBLK(i_src->i_mode))
    {
        inode_unlock_put(i_src);
        return -ENOTBLK;
    }
    inode_unlock(i_src);

    struct inode *i_target = inode_from_path(target);
    if (i_target == NULL)
    {
        inode_put(i_src);
        return -ENOENT;
    }
    inode_lock(i_target);
    if (!S_ISDIR(i_target->i_mode))
    {
        inode_put(i_src);
        inode_unlock_put(i_target);
        return -ENOTDIR;
    }
    inode_unlock(i_target);

    int ret =
        mount_types(i_src->dev, i_target, *file_system, mountflags, addr_data);

    inode_put(i_src);
    inode_put(i_target);
    return ret;
}

ssize_t mount_types(dev_t source, struct inode *i_target,
                    struct file_system_type *filesystemtype,
                    unsigned long mountflags, size_t addr_data)
{
    struct super_block *sb = get_free_super_block();
    sb->dev = source;

    void *data_kernel_space = NULL;
    if (addr_data != 0)
    {
        // addr_data is an address in user space (called via mount())
        // TODO: copy data to kernel space
        // but so far no one is using the optional data and it exists
        // for compatibility with Linux and in case it's needed later.
        panic("mount_types: implement handling of the optional data parameter");
    }

    ssize_t ret = filesystemtype->fill_super_block(sb, data_kernel_space);
    if (ret != 0)
    {
        return ret;
    }

    sb->s_mountflags = mountflags;
    sb->s_root = xv6fs_iops_iget_root(sb);
    if (i_target == NULL)
    {
        // target == NULL means this is the root file system, so it's legal
        ROOT_SUPER_BLOCK = sb;
        sb->imounted_on = NULL;
    }
    else
    {
        inode_lock(i_target);
        i_target->is_mounted_on = sb;
        sb->imounted_on = xv6fs_iops_dup(i_target);
        inode_unlock(i_target);
    }

    return 0;
}

ssize_t umount(const char *target)
{
    struct inode *i_target = inode_from_path(target);
    if (i_target == NULL)
    {
        return -ENOENT;
    }

    inode_lock(i_target);
    if (!S_ISDIR(i_target->i_mode))
    {
        inode_unlock_put(i_target);
        return -ENOTDIR;
    }

    if (i_target->i_sb->s_root != i_target)
    {
        // not the root of a mounted file system
        inode_unlock_put(i_target);
        return -EINVAL;
    }

    // TODO: Check if FS is still in use

    struct inode *i_target_mountpoint =
        xv6fs_iops_dup(i_target->i_sb->imounted_on);
    struct super_block *sb = i_target->i_sb;
    inode_unlock_put(i_target);

    DEBUG_EXTRA_ASSERT(i_target_mountpoint != NULL,
                       "imounted_on not set on mountpoint");

    inode_lock(i_target_mountpoint);
    int ret = umount_types(i_target_mountpoint, sb);
    inode_unlock_put(i_target_mountpoint);

    return ret;
}

ssize_t umount_types(struct inode *i_target_mountpoint, struct super_block *sb)
{
    DEBUG_EXTRA_ASSERT(i_target_mountpoint->is_mounted_on != NULL,
                       "imounted_on not set on mountpoint");

    // assume target to be locked
    free_super_block(sb);
    i_target_mountpoint->is_mounted_on = NULL;

    return 0;
}
