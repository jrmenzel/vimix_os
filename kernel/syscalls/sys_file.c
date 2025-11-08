/* SPDX-License-Identifier: MIT */

//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <drivers/device.h>
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

    file_dup(f);
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

    return file_read(f, buffer, n);
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

    return file_write(f, buffer, n);
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
    struct inode *ip = inode_from_path(path, &error);
    if (ip == NULL)
    {
        return error;
    }
    syserr_t ret = file_stat_by_inode(ip, stat_buffer);
    inode_put(ip);

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

    return file_stat_by_inode(f->ip, stat_buffer);
}

syserr_t sys_link()
{
    // parameter 0 / 1: const char *from / *to
    char path_to[PATH_MAX], path_from[PATH_MAX];
    if (argstr(0, path_from, PATH_MAX) < 0 || argstr(1, path_to, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return file_link(path_from, path_to);
}

syserr_t sys_unlink()
{
    // parameter 0: const char *pathname
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return file_unlink(path, true, false);
}

syserr_t sys_rmdir()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return file_unlink(path, false, true);
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

    return file_open(pathname, flags, mode);
}

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

    return inode_create(path, mode, INVALID_DEVICE);
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

    if (!dev_exists(device))
    {
        return -ENODEV;
    }

    return inode_create(path, mode, device);
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
    struct inode *ip = inode_from_path(path, &error);
    if (ip == NULL)
    {
        return error;
    }

    inode_lock(ip);
    if (!S_ISDIR(ip->i_mode))
    {
        inode_unlock_put(ip);
        return -ENOTDIR;
    }

    syserr_t perm_check = check_inode_permission(proc, ip, MAY_EXEC);
    if (perm_check < 0)
    {
        inode_unlock_put(ip);
        return perm_check;
    }

    inode_unlock(ip);

    inode_put(proc->cwd);
    proc->cwd = ip;
    return 0;
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

    return VFS_INODE_GET_DIRENT(f->ip, dir_entry_addr, true, seek_pos);
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

    syserr_t ret = file_lseek(f, offset, whence);

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
    struct inode *ip = inode_from_path(path, &error);
    if (ip == NULL)
    {
        return error;
    }

    syserr_t ret = VFS_INODE_TRUNCATE(ip, length);
    inode_put(ip);
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

    return VFS_INODE_TRUNCATE(f->ip, length);
}
