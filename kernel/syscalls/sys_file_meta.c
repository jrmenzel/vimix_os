/* SPDX-License-Identifier: MIT */

//
// File metadata management system calls.
//

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/process.h>
#include <syscalls/syscall.h>

ssize_t chmod_internal(struct inode *ip, mode_t mode)
{
    struct process *proc = get_current();

    // only superuser or file owner can change mode
    if (IS_NOT_SUPERUSER(&proc->cred) && proc->cred.euid != ip->uid)
    {
        return -EPERM;
    }

    mode = mode & ~S_IFMT;  // remove file type bits from mode parameter
    return VFS_INODE_CHMOD(ip, mode);
}

ssize_t sys_chmod()
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

    struct inode *ip = inode_from_path(path);
    if (ip == NULL)
    {
        return -ENOENT;
    }
    ssize_t ret = chmod_internal(ip, mode);
    inode_put(ip);
    return ret;
}

ssize_t sys_fchmod()
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

    return chmod_internal(f->ip, mode);
}

ssize_t chown_internal(struct inode *ip, uid_t uid, gid_t gid)
{
    struct process *proc = get_current();

    // only superuser can change owner or group
    if (IS_NOT_SUPERUSER(&proc->cred))
    {
        return -EPERM;
    }

    return VFS_INODE_CHOWN(ip, uid, gid);
}

ssize_t sys_chown()
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

    struct inode *ip = inode_from_path(path);
    if (ip == NULL)
    {
        return -ENOENT;
    }
    ssize_t ret = chown_internal(ip, uid, gid);
    inode_put(ip);
    return ret;
}

ssize_t sys_fchown()
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

    return chown_internal(f->ip, uid, gid);
}
