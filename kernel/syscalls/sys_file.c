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
#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <syscalls/syscall.h>

/// Fetch the nth word-sized system call argument as a file descriptor
/// and return both the descriptor and the corresponding struct file.
static int argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    argint(n, &fd);
    if (fd < 0 || fd >= NOFILE || (f = get_current()->files[fd]) == 0)
        return -1;
    if (pfd) *pfd = fd;
    if (pf) *pf = f;
    return 0;
}

uint64_t sys_dup()
{
    struct file *f;
    int fd;

    if (argfd(0, 0, &f) < 0) return -1;
    if ((fd = fd_alloc(f)) < 0) return -1;
    file_dup(f);
    return fd;
}

uint64_t sys_read()
{
    struct file *f;
    int n;
    uint64_t buffer;

    argaddr(1, &buffer);
    argint(2, &n);
    if (argfd(0, 0, &f) < 0) return -1;
    return file_read(f, buffer, n);
}

uint64_t sys_write()
{
    struct file *f;
    int n;
    uint64_t buffer;

    argaddr(1, &buffer);
    argint(2, &n);
    if (argfd(0, 0, &f) < 0) return -1;

    return file_write(f, buffer, n);
}

uint64_t sys_close()
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0) return -1;
    get_current()->files[fd] = 0;
    file_close(f);
    return 0;
}

uint64_t sys_fstat()
{
    struct file *f;
    uint64_t stat_buffer;  // user pointer to struct stat

    argaddr(1, &stat_buffer);
    if (argfd(0, 0, &f) < 0) return -1;
    return file_stat(f, stat_buffer);
}

/// Create the path path_to as a link to the same inode as path_from.
uint64_t sys_link()
{
    char name[XV6_NAME_MAX], path_to[MAXPATH], path_from[MAXPATH];
    struct inode *dir, *ip;

    if (argstr(0, path_from, MAXPATH) < 0 || argstr(1, path_to, MAXPATH) < 0)
        return -1;

    log_begin_fs_transaction();
    if ((ip = inode_from_path(path_from)) == 0)
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

    if ((dir = inode_of_parent_from_path(path_to, name)) == 0) goto bad;
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
    int off;
    struct xv6fs_dirent de;

    for (off = 2 * sizeof(de); off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: inode_read");
        if (de.inum != 0) return 0;
    }
    return 1;
}

uint64_t sys_unlink()
{
    struct inode *ip, *dir;
    struct xv6fs_dirent de;
    char name[XV6_NAME_MAX], path[MAXPATH];
    uint32_t off;

    if (argstr(0, path, MAXPATH) < 0) return -1;

    log_begin_fs_transaction();
    if ((dir = inode_of_parent_from_path(path, name)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }

    inode_lock(dir);

    // Cannot unlink "." or "..".
    if (file_name_cmp(name, ".") == 0 || file_name_cmp(name, "..") == 0)
        goto bad;

    if ((ip = inode_dir_lookup(dir, name, &off)) == 0) goto bad;
    inode_lock(ip);

    if (ip->nlink < 1) panic("unlink: nlink < 1");
    if (ip->type == XV6_FT_DIR && !isdirempty(ip))
    {
        inode_unlock_put(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (inode_write(dir, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
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

    if ((dir = inode_of_parent_from_path(path, name)) == 0) return 0;

    inode_lock(dir);

    if ((ip = inode_dir_lookup(dir, name, 0)) != 0)
    {
        inode_unlock_put(dir);
        inode_lock(ip);
        if (type == XV6_FT_FILE &&
            (ip->type == XV6_FT_FILE || ip->type == XV6_FT_DEVICE))
            return ip;
        inode_unlock_put(ip);
        return 0;
    }

    if ((ip = inode_alloc(dir->dev, type)) == 0)
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

uint64_t sys_open()
{
    char path[MAXPATH];
    int fd, flags;
    struct file *f;
    struct inode *ip;
    int n;

    argint(1, &flags);
    if ((n = argstr(0, path, MAXPATH)) < 0) return -1;

    log_begin_fs_transaction();

    if (flags & O_CREATE)
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
        if ((ip = inode_from_path(path)) == 0)
        {
            log_end_fs_transaction();
            return -1;
        }
        inode_lock(ip);
        if (ip->type == XV6_FT_DIR && flags != O_RDONLY)
        {
            inode_unlock_put(ip);
            log_end_fs_transaction();
            return -1;
        }
    }

    if (ip->type == XV6_FT_DEVICE && (ip->major < 0 || ip->major >= NDEV))
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
    f->readable = !(flags & O_WRONLY);
    f->writable = (flags & O_WRONLY) || (flags & O_RDWR);

    if ((flags & O_TRUNC) && ip->type == XV6_FT_FILE)
    {
        inode_trunc(ip);
    }

    inode_unlock(ip);
    log_end_fs_transaction();

    return fd;
}

uint64_t sys_mkdir()
{
    char path[MAXPATH];
    struct inode *ip;

    log_begin_fs_transaction();
    if (argstr(0, path, MAXPATH) < 0 ||
        (ip = create(path, XV6_FT_DIR, 0, 0)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    return 0;
}

uint64_t sys_mknod()
{
    struct inode *ip;
    char path[MAXPATH];
    int major, minor;

    log_begin_fs_transaction();
    argint(1, &major);
    argint(2, &minor);
    if ((argstr(0, path, MAXPATH)) < 0 ||
        (ip = create(path, XV6_FT_DEVICE, major, minor)) == 0)
    {
        log_end_fs_transaction();
        return -1;
    }
    inode_unlock_put(ip);
    log_end_fs_transaction();
    return 0;
}

uint64_t sys_chdir()
{
    char path[MAXPATH];
    struct inode *ip;
    struct process *proc = get_current();

    log_begin_fs_transaction();
    if (argstr(0, path, MAXPATH) < 0 || (ip = inode_from_path(path)) == 0)
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

uint64_t sys_execv()
{
    char path[MAXPATH], *argv[MAX_EXEC_ARGS];
    int i;
    uint64_t uargv, uarg;

    argaddr(1, &uargv);
    if (argstr(0, path, MAXPATH) < 0)
    {
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for (i = 0;; i++)
    {
        if (i >= NELEM(argv))
        {
            goto bad;
        }
        if (fetchaddr(uargv + sizeof(uint64_t) * i, (uint64_t *)&uarg) < 0)
        {
            goto bad;
        }
        if (uarg == 0)
        {
            argv[i] = 0;
            break;
        }
        argv[i] = kalloc();
        if (argv[i] == 0) goto bad;
        if (fetchstr(uarg, argv[i], PAGE_SIZE) < 0) goto bad;
    }

    int ret = execv(path, argv);

    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++) kfree(argv[i]);

    return ret;

bad:
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++) kfree(argv[i]);
    return -1;
}
