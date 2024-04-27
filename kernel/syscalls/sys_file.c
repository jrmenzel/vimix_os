/* SPDX-License-Identifier: MIT */

//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <arch/riscv/mm/mm.h>
#include <fs/xv6fs/log.h>
#include <ipc/pipe.h>
#include <kernel/exec.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <kernel/vm.h>
#include <syscalls/syscall.h>

/// Fetch the nth word-sized system call argument as a file descriptor
/// and return both the descriptor and the corresponding struct file.
static int argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    argint(n, &fd);
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS ||
        (f = get_current()->files[fd]) == NULL)
        return -1;
    if (pfd) *pfd = fd;
    if (pf) *pf = f;
    return 0;
}

size_t sys_dup()
{
    struct file *f;
    int fd;

    if (argfd(0, NULL, &f) < 0) return -1;
    if ((fd = fd_alloc(f)) < 0) return -1;
    file_dup(f);
    return fd;
}

size_t sys_read()
{
    struct file *f;
    size_t buffer;

    argaddr(1, &buffer);

    int32_t n;
    argint(2, &n);
    if (argfd(0, NULL, &f) < 0) return -1;
    return file_read(f, buffer, n);
}

size_t sys_write()
{
    struct file *f;
    size_t buffer;
    argaddr(1, &buffer);

    int32_t n;
    argint(2, &n);
    if (argfd(0, NULL, &f) < 0) return -1;

    return file_write(f, buffer, n);
}

size_t sys_close()
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0) return -1;
    get_current()->files[fd] = NULL;
    file_close(f);
    return 0;
}

size_t sys_fstat()
{
    struct file *f;
    size_t stat_buffer;  // user pointer to struct stat

    argaddr(1, &stat_buffer);
    if (argfd(0, NULL, &f) < 0) return -1;
    return file_stat(f, stat_buffer);
}

size_t sys_link()
{
    char name[XV6_NAME_MAX], path_to[PATH_MAX], path_from[PATH_MAX];
    struct inode *dir, *ip;

    if (argstr(0, path_from, PATH_MAX) < 0 || argstr(1, path_to, PATH_MAX) < 0)
        return -1;

    log_begin_fs_transaction();
    if ((ip = inode_from_path(path_from)) == NULL)
    {
        log_end_fs_transaction();
        return -1;
    }

    inode_lock(ip);
    if (ip->type == XV6_FT_DIR)
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -1;
    }

    ip->nlink++;
    inode_update(ip);
    inode_unlock(ip);

    if ((dir = inode_of_parent_from_path(path_to, name)) == NULL) goto bad;
    inode_lock(dir);
    if (dir->dev != ip->dev || inode_dir_link(dir, name, ip->inum) < 0)
    {
        inode_unlock_put(dir);
        goto bad;
    }
    inode_unlock_put(dir);
    inode_put(ip);

    log_end_fs_transaction();

    return 0;

bad:
    inode_lock(ip);
    ip->nlink--;
    inode_update(ip);
    inode_unlock_put(ip);
    log_end_fs_transaction();
    return -1;
}

/// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dir)
{
    struct xv6fs_dirent de;

    for (size_t off = 2 * sizeof(de); off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: inode_read");
        if (de.inum != 0) return 0;
    }
    return 1;
}

size_t sys_unlink()
{
    struct inode *ip, *dir;
    struct xv6fs_dirent de;
    char name[XV6_NAME_MAX], path[PATH_MAX];
    uint32_t off;

    if (argstr(0, path, PATH_MAX) < 0) return -1;

    log_begin_fs_transaction();
    if ((dir = inode_of_parent_from_path(path, name)) == NULL)
    {
        log_end_fs_transaction();
        return -1;
    }

    inode_lock(dir);

    // Cannot unlink "." or "..".
    if (file_name_cmp(name, ".") == 0 || file_name_cmp(name, "..") == 0)
        goto bad;

    if ((ip = inode_dir_lookup(dir, name, &off)) == NULL) goto bad;
    inode_lock(ip);

    if (ip->nlink < 1) panic("unlink: nlink < 1");
    if (ip->type == XV6_FT_DIR && !isdirempty(ip))
    {
        inode_unlock_put(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (inode_write(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
        panic("unlink: inode_write");
    if (ip->type == XV6_FT_DIR)
    {
        dir->nlink--;
        inode_update(dir);
    }
    inode_unlock_put(dir);

    ip->nlink--;
    inode_update(ip);
    inode_unlock_put(ip);

    log_end_fs_transaction();

    return 0;

bad:
    inode_unlock_put(dir);
    log_end_fs_transaction();
    return -1;
}

static struct inode *create(char *path, short type, short major, short minor)
{
    struct inode *ip, *dir;
    char name[XV6_NAME_MAX];

    if ((dir = inode_of_parent_from_path(path, name)) == NULL) return 0;

    inode_lock(dir);

    if ((ip = inode_dir_lookup(dir, name, 0)) != NULL)
    {
        inode_unlock_put(dir);
        inode_lock(ip);
        if (type == XV6_FT_FILE &&
            (ip->type == XV6_FT_FILE || ip->type == XV6_FT_DEVICE))
            return ip;
        inode_unlock_put(ip);
        return 0;
    }

    if ((ip = inode_alloc(dir->dev, type)) == NULL)
    {
        inode_unlock_put(dir);
        return 0;
    }

    inode_lock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    inode_update(ip);

    if (type == XV6_FT_DIR)
    {  // Create . and .. entries.
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (inode_dir_link(ip, ".", ip->inum) < 0 ||
            inode_dir_link(ip, "..", dir->inum) < 0)
            goto fail;
    }

    if (inode_dir_link(dir, name, ip->inum) < 0) goto fail;

    if (type == XV6_FT_DIR)
    {
        // now that success is guaranteed:
        dir->nlink++;  // for ".."
        inode_update(dir);
    }

    inode_unlock_put(dir);

    return ip;

fail:
    // something went wrong. de-allocate ip.
    ip->nlink = 0;
    inode_update(ip);
    inode_unlock_put(ip);
    inode_unlock_put(dir);
    return 0;
}

size_t sys_open()
{
    char path[PATH_MAX];
    int fd, omode;
    struct file *f;
    struct inode *ip;
    int n;

    argint(1, &omode);
    if ((n = argstr(0, path, PATH_MAX)) < 0) return -1;

    log_begin_fs_transaction();

    if (omode & O_CREATE)
    {
        ip = create(path, XV6_FT_FILE, 0, 0);
        if (ip == 0)
        {
            log_end_fs_transaction();
            return -1;
        }
    }
    else
    {
        if ((ip = inode_from_path(path)) == NULL)
        {
            log_end_fs_transaction();
            return -1;
        }
        inode_lock(ip);
        if (ip->type == XV6_FT_DIR && omode != O_RDONLY)
        {
            inode_unlock_put(ip);
            log_end_fs_transaction();
            return -1;
        }
    }

    if (ip->type == XV6_FT_DEVICE &&
        (ip->major < 0 || ip->major >= MAX_DEVICES))
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -1;
    }

    if ((f = file_alloc()) == 0 || (fd = fd_alloc(f)) < 0)
    {
        if (f) file_close(f);
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -1;
    }

    if (ip->type == XV6_FT_DEVICE)
    {
        f->type = FD_DEVICE;
        f->major = ip->major;
    }
    else
    {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->ip = ip;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

    if ((omode & O_TRUNC) && ip->type == XV6_FT_FILE)
    {
        inode_trunc(ip);
    }

    inode_unlock(ip);
    log_end_fs_transaction();

    return fd;
}

size_t sys_mkdir()
{
    char path[PATH_MAX];
    struct inode *ip;

    log_begin_fs_transaction();
    if (argstr(0, path, PATH_MAX) < 0 ||
        (ip = create(path, XV6_FT_DIR, 0, 0)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    return 0;
}

size_t sys_mknod()
{
    struct inode *ip;
    char path[PATH_MAX];
    int major, minor;

    log_begin_fs_transaction();
    argint(1, &major);
    argint(2, &minor);
    if ((argstr(0, path, PATH_MAX)) < 0 ||
        (ip = create(path, XV6_FT_DEVICE, major, minor)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    return 0;
}

size_t sys_chdir()
{
    char path[PATH_MAX];
    struct inode *ip;
    struct process *proc = get_current();

    log_begin_fs_transaction();
    if (argstr(0, path, PATH_MAX) < 0 || (ip = inode_from_path(path)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_lock(ip);
    if (ip->type != XV6_FT_DIR)
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -1;
    }
    inode_unlock(ip);
    inode_put(proc->cwd);
    log_end_fs_transaction();
    proc->cwd = ip;
    return 0;
}

size_t sys_execv()
{
    char path[PATH_MAX], *argv[MAX_EXEC_ARGS];
    size_t uargv, uarg;

    argaddr(1, &uargv);
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for (size_t i = 0;; i++)
    {
        if (i >= NELEM(argv))
        {
            goto bad;
        }
        if (fetchaddr(uargv + sizeof(size_t) * i, (size_t *)&uarg) < 0)
        {
            goto bad;
        }
        if (uarg == 0)
        {
            argv[i] = 0;
            break;
        }
        argv[i] = kalloc();
        if (argv[i] == NULL) goto bad;
        if (fetchstr(uarg, argv[i], PAGE_SIZE) < 0) goto bad;
    }

    size_t ret = execv(path, argv);

    for (size_t i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    {
        kfree(argv[i]);
    }
    return ret;

bad:
    for (size_t i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    {
        kfree(argv[i]);
    }
    return -1;
}
