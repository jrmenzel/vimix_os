/* SPDX-License-Identifier: MIT */

//
// mkdir and mknod system calls.
//

#include <drivers/device.h>
#include <fs/fs_lookup.h>
#include <kernel/dirent.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>
#include <kernel/proc.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <syscalls/syscall.h>

syserr_t do_mkdir(const char *pathname, mode_t mode);
syserr_t do_mknod(const char *pathname, mode_t mode, dev_t device);

syserr_t sys_mkdir()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    uint32_t could_fetch_path = argstr(0, path, PATH_MAX);
    if (could_fetch_path < 0)
    {
        return -EFAULT;
    }

    // parameter 1: mode_t mode
    mode_t mode;
    arguint(1, &mode);
    if (!check_and_adjust_mode(&mode, S_IFDIR) || !S_ISDIR(mode))
    {
        return -ENOTDIR;
    }

    return do_mkdir(path, mode);
}

syserr_t sys_mknod()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    uint32_t could_fetch_path = argstr(0, path, PATH_MAX);
    if (could_fetch_path < 0)
    {
        return -EFAULT;
    }

    // parameter 1: mode_t mode
    mode_t mode;
    arguint(1, &mode);
    if (!check_and_adjust_mode(&mode, S_IFREG))
    {
        return -EINVAL;
    }

    // parameter 2: dev_t device
    dev_t device;
    argint(2, &device);

    return do_mknod(path, mode, device);
}

syserr_t do_mkdir(const char *pathname, mode_t mode)
{
    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(pathname, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_valid(dp))
    {
        dentry_put(dp);
        return -EEXIST;
    }

    dentry_lock(dp);
    struct dentry *parent_dp = dentry_get(dp->parent);
    struct inode *parent_ip = parent_dp->ip;
    dentry_unlock(dp);

    syserr_t perm_ok =
        check_dentry_permission(get_current(), parent_dp, MAY_WRITE);
    if (perm_ok < 0)
    {
        dentry_put(dp);
        dentry_put(parent_dp);
        return perm_ok;
    }

    inode_lock_exclusive(parent_ip);
    syserr_t err = VFS_INODE_MKDIR(parent_ip, dp, mode);
    inode_unlock_exclusive(parent_ip);

    dentry_put(parent_dp);
    dentry_put(dp);

    return err;
}

syserr_t do_mknod(const char *pathname, mode_t mode, dev_t device)
{
    if (!dev_exists(device))
    {
        return -ENODEV;
    }

    syserr_t error = 0;

    struct dentry *dp = dentry_from_path(pathname, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_valid(dp))
    {
        dentry_put(dp);
        return -EEXIST;
    }

    dentry_lock(dp);
    struct dentry *parent_dp = dentry_get(dp->parent);
    dentry_unlock(dp);

    syserr_t perm_ok =
        check_dentry_permission(get_current(), parent_dp, MAY_WRITE);
    if (perm_ok < 0)
    {
        dentry_put(dp);
        dentry_put(parent_dp);
        return perm_ok;
    }

    inode_lock_exclusive(parent_dp->ip);
    syserr_t err = VFS_INODE_MKNOD(parent_dp->ip, dp, mode, device);
    inode_unlock_exclusive(parent_dp->ip);

    dentry_put(parent_dp);
    dentry_put(dp);

    return err;
}
