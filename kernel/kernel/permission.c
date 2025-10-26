/* SPDX-License-Identifier: MIT */

//
// Permission control functions.
//

#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>

ssize_t check_file_permission(struct process *proc, struct file *f,
                              perm_mask_t mask)
{
    bool needs_write_access = (mask & MAY_WRITE) || (mask & MAY_APPEND);
    bool needs_read_access = (mask & MAY_READ) || (mask & MAY_EXEC);

    // note that root does not skip file open mode checks

    if ((f->flags == O_RDONLY) && needs_write_access)
    {
        return -EBADF;
    }
    else if ((f->flags == O_WRONLY) && needs_read_access)
    {
        return -EBADF;
    }

    return 0;
}

bool in_group(struct cred *cred, gid_t gid)
{
    if (gid == cred->egid)
    {
        return true;
    }
    for (size_t i = 0; i < cred->groups->ngroups; i++)
    {
        if (cred->groups->gid[i] == gid)
        {
            return true;
        }
    }
    return false;
}

bool permission_check_read(struct cred *cred, mode_t mode, uid_t uid, gid_t gid)
{
    if (cred->euid == 0)
    {
        // root can always read
        return true;
    }

    if (cred->euid == uid)
    {
        return (mode & S_IRUSR);
    }
    else if (in_group(cred, gid))
    {
        return (mode & S_IRGRP);
    }

    // fall back to other
    return (mode & S_IROTH);
}

bool permission_check_write(struct cred *cred, mode_t mode, uid_t uid,
                            gid_t gid)
{
    if (cred->euid == 0)
    {
        // root can always write
        return true;
    }

    if (cred->euid == uid)
    {
        return (mode & S_IWUSR);
    }
    else if (in_group(cred, gid))
    {
        return (mode & S_IWGRP);
    }

    // fall back to other
    return (mode & S_IWOTH);
}

bool permission_check_execute(struct cred *cred, mode_t mode, uid_t uid,
                              gid_t gid)
{
    // execute bit is even checked for root!

    if (cred->euid == uid)
    {
        return (mode & S_IXUSR);
    }
    else if (in_group(cred, gid))
    {
        return (mode & S_IXGRP);
    }

    // fall back to other
    return (mode & S_IXOTH);
}

ssize_t check_inode_permission(struct process *proc, struct inode *ip,
                               int32_t flags)
{
    bool needs_write_access = (flags & O_APPEND) || (flags & O_CREAT) ||
                              (flags & O_TRUNC) || (flags & O_WRONLY) ||
                              (flags & O_RDWR);
    // note: O_RDONLY is 0, not a set bit!
    bool needs_read_access =
        (flags == O_RDONLY) || (flags & O_RDWR) || (flags & O_EXEC);

    if (needs_read_access)
    {
        if (!permission_check_read(&proc->cred, ip->i_mode, ip->uid, ip->gid))
        {
            return -EACCES;
        }
    }
    if (needs_write_access)
    {
        if (!permission_check_write(&proc->cred, ip->i_mode, ip->uid, ip->gid))
        {
            return -EACCES;
        }
    }
    if (flags & O_EXEC)
    {
        if (!permission_check_execute(&proc->cred, ip->i_mode, ip->uid,
                                      ip->gid))
        {
            return -EACCES;
        }
    }

    return 0;
}