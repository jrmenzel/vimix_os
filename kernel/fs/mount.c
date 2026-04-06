/* SPDX-License-Identifier: MIT */

#include <fs/dentry_cache.h>
#include <fs/devfs/devfs.h>
#include <fs/fs_lookup.h>
#include <fs/vfs.h>
#include <fs/vimixfs/vimixfs.h>
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
#include <mm/kalloc.h>

/// @brief Device number of the root file system. Set in main() during hardware
/// discovery and used once during FS init / mount_root()
dev_t ROOT_DEVICE_NUMBER;

/// @brief Super block of the root file system, set in mount_root() and used
/// during FS tree traversal in namex()
struct super_block *ROOT_SUPER_BLOCK = NULL;

/// @brief Lock to protect the mount/umount "inner" calls (after input
/// validation / error checks). This means only one process can mount or umount
/// at a time, but that limitation is fine.
struct sleeplock g_mount_lock;

syserr_t do_mount(const char *source, const char *target,
                  const char *filesystemtype, unsigned long mountflags,
                  size_t addr_data)
{
    struct file_system_type **file_system =
        find_filesystem(filesystemtype, strlen(filesystemtype));

    if (*file_system == NULL)
    {
        return -EINVAL;
    }

    // source is a path to a block device, e.g. "/dev/sda1"
    // but in special cases it is a magic constant
    if ((strcmp(source, "dev") == 0) || (strcmp(source, "sys") == 0))
    {
        syserr_t error = 0;
        struct dentry *d_target = dentry_from_path(target, &error);
        if (d_target == NULL)
        {
            return error;
        }
        if (dentry_is_invalid(d_target))
        {
            dentry_put(d_target);
            return -ENOENT;
        }

        if (!S_ISDIR(d_target->ip->i_mode))
        {
            dentry_put(d_target);
            return -ENOTDIR;
        }

        sleep_lock(&g_mount_lock);
        syserr_t ret = mount_internal(INVALID_DEVICE, d_target, *file_system,
                                      mountflags, addr_data);

        dentry_put(d_target);
        sleep_unlock(&g_mount_lock);
        return ret;
    }

    // normal filesystem on a device:

    syserr_t error = 0;
    struct dentry *d_src = dentry_from_path(source, &error);
    if (d_src == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(d_src))
    {
        dentry_put(d_src);
        return -ENOENT;
    }

    if (!S_ISBLK(d_src->ip->i_mode))
    {
        dentry_put(d_src);
        return -ENOTBLK;
    }

    struct dentry *d_target = dentry_from_path(target, &error);
    if (d_target == NULL)
    {
        dentry_put(d_src);
        return error;
    }
    if (dentry_is_invalid(d_target))
    {
        dentry_put(d_target);
        return -ENOENT;
    }

    if (!S_ISDIR(d_target->ip->i_mode))
    {
        dentry_put(d_src);
        dentry_put(d_target);
        return -ENOTDIR;
    }

    inode_lock_exclusive(d_target->ip);
    sleep_lock(&g_mount_lock);
    syserr_t ret = mount_internal(d_src->ip->dev, d_target, *file_system,
                                  mountflags, addr_data);

    sleep_unlock(&g_mount_lock);
    inode_unlock_exclusive(d_target->ip);

    dentry_put(d_src);
    dentry_put(d_target);

    return ret;
}

void mount_root(dev_t dev, const char *filesystemtype)
{
    struct file_system_type **file_system =
        find_filesystem(filesystemtype, strlen(filesystemtype));

    if (*file_system == NULL)
    {
        printk("no support for file system %s\n", filesystemtype);
        panic("root file system init failed");
    }

    sleep_lock(&g_mount_lock);
    unsigned long flags = 0;
    syserr_t ret = mount_internal(dev, NULL, *file_system, flags, 0);
    sleep_unlock(&g_mount_lock);
    if (ret != 0)
    {
        printk("mount_internal() returned error %zd\n", -ret);
        panic("root file system init failed, could not mount /");
    }
}

syserr_t mount_internal(dev_t source, struct dentry *d_target,
                        struct file_system_type *filesystemtype,
                        unsigned long mountflags, size_t addr_data)
{
    struct super_block *sb = sb_alloc_init();
    if (sb == NULL) return -ENOMEM;

    sb->dev = source;

    void *data_kernel_space = NULL;
    if (addr_data != 0)
    {
        // addr_data is an address in user space (called via mount())
        // TODO: copy data to kernel space
        // but so far no one is using the optional data and it exists
        // for compatibility with Linux and in case it's needed later.
        panic(
            "mount_internal: implement handling of the optional data "
            "parameter");
    }

    syserr_t ret = filesystemtype->init_fs_super_block(sb, data_kernel_space);
    if (ret != 0)
    {
        return ret;
    }

    sb->s_mountflags = mountflags;
    struct inode *root_ip = VFS_SUPER_IGET_ROOT(sb);
    sb->s_root = root_ip;
    struct dentry *root_dp = NULL;
    if (d_target == NULL)
    {
        // target == NULL means this is the root file system, so it's legal
        ROOT_SUPER_BLOCK = sb;
        sb->imounted_on = NULL;
        sb->dmounted_on = NULL;
        root_dp = dentry_cache_init(root_ip);
    }
    else
    {
        inode_lock(d_target->ip);
        d_target->ip->is_mounted_on = sb;
        sb->imounted_on = inode_get(d_target->ip);
        sb->dmounted_on = dentry_get(d_target);
        inode_unlock(d_target->ip);

        struct dentry *new_target =
            dentry_alloc_init_orphan(d_target->name, root_ip);
        dentry_switch_children(d_target, new_target);
        root_dp = new_target;
    }
    sb->d_root = root_dp;

    // add to kobject tree
    kobject_add(&sb->kobj, &g_kobjects_fs, "%s_(%d,%d)", sb->s_type->name,
                MAJOR(sb->dev), MINOR(sb->dev));
    kobject_put(
        &sb->kobj);  // drop reference now that the kobject tree holds one

    return 0;
}

syserr_t do_umount(const char *target)
{
    syserr_t error = 0;
    struct dentry *d_target = dentry_from_path(target, &error);
    if (d_target == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(d_target))
    {
        dentry_put(d_target);
        return -ENOENT;
    }

    if (!S_ISDIR(d_target->ip->i_mode))
    {
        dentry_put(d_target);
        return -ENOTDIR;
    }

    inode_lock(d_target->ip);
    struct super_block *sb = d_target->ip->i_sb;

    if (sb->d_root != d_target)
    {
        // not the root of a mounted file system
        inode_unlock(d_target->ip);
        dentry_put(d_target);
        return -EINVAL;
    }

    if (sb->dmounted_on == NULL)
    {
        // this is the root file system -> don't unmount
        inode_unlock(d_target->ip);
        dentry_put(d_target);
        return -EACCES;
    }

    struct dentry *d_target_mountpoint = dentry_get(sb->dmounted_on);
    inode_unlock(d_target->ip);

    DEBUG_EXTRA_ASSERT(d_target_mountpoint != NULL,
                       "dmounted_on not set on mountpoint");

    // TODO: Check if FS is still in use

    inode_lock_exclusive(d_target_mountpoint->ip);
    sleep_lock(&g_mount_lock);
    syserr_t ret = umount_internal(d_target, d_target_mountpoint, sb);
    sleep_unlock(&g_mount_lock);
    inode_unlock_exclusive(d_target_mountpoint->ip);
    inode_put(d_target_mountpoint->ip);

    dentry_put(d_target);

    return ret;
}

syserr_t umount_internal(struct dentry *d_target,
                         struct dentry *d_target_mountpoint,
                         struct super_block *sb)
{
    DEBUG_EXTRA_ASSERT(d_target_mountpoint->ip->is_mounted_on != NULL,
                       "imounted_on not set on mountpoint");

    inode_lock(d_target_mountpoint->ip);
    sb->s_type->kill_sb(sb);  // free file system specific data
    sb_free(sb);              // free generic super block itself
    d_target_mountpoint->ip->is_mounted_on = NULL;

    dentry_switch_children(d_target, d_target_mountpoint);
    inode_unlock(d_target_mountpoint->ip);

    return 0;
}
