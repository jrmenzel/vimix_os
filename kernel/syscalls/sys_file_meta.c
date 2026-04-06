/* SPDX-License-Identifier: MIT */

//
// File metadata management system calls.
//

#include <fs/fs_lookup.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <syscalls/syscall.h>

syserr_t do_chmod(struct dentry *dp, mode_t mode);
syserr_t do_chown(struct dentry *dp, uid_t uid, gid_t gid);
syserr_t do_stat(struct dentry *dp, size_t addr);

syserr_t sys_chmod()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: mode_t mode
    int32_t mode;
    argint(1, &mode);

    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(path, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dp))
    {
        dentry_put(dp);
        return -ENOENT;
    }

    syserr_t ret = do_chmod(dp, mode);
    dentry_put(dp);
    return ret;
}

syserr_t sys_fchmod()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: mode_t mode
    int32_t mode;
    argint(1, &mode);

    syserr_t ret = do_chmod(f->dp, mode);
    f->mode = f->dp->ip->i_mode;  // update cached mode in file struct

    return ret;
}

syserr_t sys_chown()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: uid_t uid
    int32_t uid;
    argint(1, &uid);

    // parameter 2: gid_t gid
    int32_t gid;
    argint(2, &gid);

    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(path, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dp))
    {
        dentry_put(dp);
        return -ENOENT;
    }

    syserr_t ret = do_chown(dp, uid, gid);
    dentry_put(dp);
    return ret;
}

syserr_t sys_fchown()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: uid_t uid
    int32_t uid;
    argint(1, &uid);

    // parameter 2: gid_t gid
    int32_t gid;
    argint(2, &gid);

    return do_chown(f->dp, uid, gid);
}

syserr_t sys_umask()
{
    // parameter 0: mode_t (int32_t) mask
    int32_t mask;
    argint(0, &mask);

    struct process *proc = get_current();
    mode_t old_mask = proc->umask;
    proc->umask = mask & 0777;  // only permission bits

    return (syserr_t)old_mask;
}

syserr_t sys_stat()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: stat *buffer
    size_t stat_buffer;  // user pointer to struct stat
    argaddr(1, &stat_buffer);

    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(path, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dp))
    {
        dentry_put(dp);
        return -ENOENT;
    }

    syserr_t ret = do_stat(dp, stat_buffer);
    dentry_put(dp);

    return ret;
}

syserr_t sys_fstat()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }
    DEBUG_EXTRA_PANIC(INODE_HAS_TYPE(f->mode), "sys_fstat(): file has no type");

    // parameter 1: stat *buffer
    size_t stat_buffer;  // user pointer to struct stat
    argaddr(1, &stat_buffer);

    return do_stat(f->dp, stat_buffer);
}

syserr_t do_chmod(struct dentry *dp, mode_t mode)
{
    // only superuser or file owner can change mode
    ASSERT_IS_FILE_OWNER(get_current(), dp->ip);

    mode = mode & ~S_IFMT;  // remove file type bits from mode parameter
    syserr_t ret = VFS_INODE_CHMOD(dp, mode);
    return ret;
}

syserr_t do_chown(struct dentry *dp, uid_t uid, gid_t gid)
{
    struct process *proc = get_current();

    // only superuser can change owner or group
    if (IS_NOT_SUPERUSER(&proc->cred))
    {
        return -EPERM;
    }

    return VFS_INODE_CHOWN(dp, uid, gid);
}

/// @brief Get metadata about file.
/// @param dp dentry of file to get stats from.
/// @param addr a user virtual address, pointing to a struct stat.
/// @return 0 on success, -1 on error
syserr_t do_stat(struct dentry *dp, size_t addr)
{
    struct stat st;
    struct process *proc = get_current();
    inode_lock(dp->ip);
    inode_stat(dp->ip, &st);
    inode_unlock(dp->ip);
    if (uvm_copy_out(proc->pagetable, addr, (char *)&st, sizeof(st)) < 0)
    {
        return -EFAULT;
    }
    return 0;
}
