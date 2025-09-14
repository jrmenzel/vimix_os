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
#include <kernel/proc.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <syscalls/syscall.h>

ssize_t sys_dup()
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
    return fd;
}

ssize_t sys_read()
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

ssize_t sys_write()
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

ssize_t sys_close()
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

ssize_t sys_fstat()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: stat *buffer
    size_t stat_buffer;  // user pointer to struct stat
    argaddr(1, &stat_buffer);

    return file_stat(f, stat_buffer);
}

ssize_t sys_link()
{
    // parameter 0 / 1: const char *from / *to
    char path_to[PATH_MAX], path_from[PATH_MAX];
    if (argstr(0, path_from, PATH_MAX) < 0 || argstr(1, path_to, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return file_link(path_from, path_to);
}

ssize_t sys_unlink()
{
    // parameter 0: const char *pathname
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return file_unlink(path, true, false);
}

ssize_t sys_rmdir()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return file_unlink(path, false, true);
}

ssize_t sys_open()
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

ssize_t sys_mkdir()
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

    return (size_t)inode_create(path, mode, INVALID_DEVICE);
}

ssize_t sys_mknod()
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

    return (size_t)inode_create(path, mode, device);
}

ssize_t sys_chdir()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    uint32_t could_fetch_path = argstr(0, path, PATH_MAX);
    if (could_fetch_path < 0)
    {
        return -EFAULT;
    }

    struct process *proc = get_current();
    struct inode *ip = inode_from_path(path);
    if (ip == NULL)
    {
        return -ENOENT;
    }

    inode_lock(ip);
    if (!S_ISDIR(ip->i_mode))
    {
        inode_unlock_put(ip);
        return -ENOTDIR;
    }
    inode_unlock(ip);

    inode_put(proc->cwd);
    proc->cwd = ip;
    return 0;
}

ssize_t sys_get_dirent()
{
    // size_t get_dirent(int fd, struct dirent *dirp, size_t seek_pos);
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

ssize_t sys_lseek()
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

    ssize_t ret = file_lseek(f, offset, whence);

    return ret;
}
