/* SPDX-License-Identifier: MIT */

//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <drivers/device.h>
#include <fs/dentry_cache.h>
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

syserr_t sys_dup()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    FILE_DESCRIPTOR fd = fd_alloc(f);
    if (fd < 0)
    {
        return -EMFILE;
    }
    file_get(f);

    return (syserr_t)fd;
}

syserr_t sys_read()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: void *buffer
    size_t buffer;

    argaddr(1, &buffer);

    // parameter 2: n
    size_t n;
    argsize_t(2, &n);

    return do_read(f, buffer, n);
}

syserr_t sys_write()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: void *buffer
    size_t buffer;
    argaddr(1, &buffer);

    // parameter 2: n
    size_t n;
    argsize_t(2, &n);

    return do_write(f, buffer, n);
}

syserr_t sys_close()
{
    // parameter 0: int fd
    FILE_DESCRIPTOR fd;
    struct file *f;
    if (argfd(0, &fd, &f) < 0)
    {
        return -EBADF;
    }

    get_current()->files[fd] = NULL;
    file_close(f);
    return 0;
}

syserr_t sys_open()
{
    // parameter 0: const char *pathname
    char pathname[PATH_MAX];
    int32_t n = argstr(0, pathname, PATH_MAX);
    if (n < 0)
    {
        return -EFAULT;
    }

    // parameter 1: int32_t flags
    int32_t flags;
    argint(1, &flags);

    // optional parameter 2: mode_t mode - only used when creating a file:
    mode_t mode;
    arguint(2, &mode);

    syserr_t ret = do_open(pathname, flags, mode);
    return ret;
}

syserr_t sys_chdir()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    uint32_t could_fetch_path = argstr(0, path, PATH_MAX);
    if (could_fetch_path < 0)
    {
        return -EFAULT;
    }

    syserr_t error = 0;
    struct process *proc = get_current();
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

    if (!S_ISDIR(dp->ip->i_mode))
    {
        dentry_put(dp);
        return -ENOTDIR;
    }

    syserr_t perm_check = check_dentry_permission(proc, dp, MAY_EXEC);
    if (perm_check < 0)
    {
        dentry_put(dp);
        return perm_check;
    }

    dentry_put(proc->cwd_dentry);
    dentry_lock(dp);
    if (dp->parent == NULL)
    {
        // might be an unlinked directory ot root, in both cases use the root
        proc->cwd_dentry = dentry_cache_get_root();
    }
    else
    {
        proc->cwd_dentry = dentry_get(dp);
    }
    dentry_unlock(dp);
    dentry_put(dp);

    return 0;
}

syserr_t do_get_dirent(struct file *f, size_t dir_entry_addr, ssize_t seek_pos)
{
    if (!S_ISDIR(f->mode))
    {
        return -ENOTDIR;
    }
    if (seek_pos < 0)
    {
        return -EINVAL;
    }

    struct dirent dir_entry;
    syserr_t ret = VFS_INODE_GET_DIRENT(f->dp->ip, &dir_entry, seek_pos);
    if (ret > 0)
    {
        int32_t res = either_copyout(true, dir_entry_addr, (void *)&dir_entry,
                                     sizeof(struct dirent));
        if (res < 0) return -EFAULT;
    }

    return ret;
}

syserr_t sys_get_dirent()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: struct dirent *dirp
    size_t dir_entry_addr;
    argaddr(1, &dir_entry_addr);

    // parameter 2: seek_pos
    ssize_t seek_pos;
    argssize_t(2, &seek_pos);

    return do_get_dirent(f, dir_entry_addr, seek_pos);
}

syserr_t sys_lseek()
{
    // parameter 0: int fd
    FILE_DESCRIPTOR fd;
    struct file *f;
    if (argfd(0, &fd, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: off_t offset
    ssize_t offset;
    argssize_t(1, &offset);

    // parameter 2: int whence
    int32_t whence;
    argint(2, &whence);

    syserr_t ret = do_lseek(f, offset, whence);

    return ret;
}

static inline syserr_t do_truncate(struct dentry *dp, ssize_t length)
{
    inode_lock_exclusive(dp->ip);
    syserr_t ret = VFS_INODE_TRUNCATE(dp, length);
    inode_unlock_exclusive(dp->ip);
    return ret;
}

syserr_t sys_truncate()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: off_t length
    ssize_t length;
    argssize_t(1, &length);

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

    syserr_t ret = do_truncate(dp, length);
    dentry_put(dp);
    return ret;
}

syserr_t sys_ftruncate()
{
    // parameter 0: int fd
    FILE_DESCRIPTOR fd;
    struct file *f;
    if (argfd(0, &fd, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: off_t length
    ssize_t length;
    argssize_t(1, &length);

    return do_truncate(f->dp, length);
}
