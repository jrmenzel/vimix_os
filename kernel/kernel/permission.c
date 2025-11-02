/* SPDX-License-Identifier: MIT */

//
// Permission control functions.
//

#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>

perm_mask_t perm_mask_from_open_flags(int32_t flags)
{
    perm_mask_t mask = 0;

    if (flags == O_RDONLY)
    {
        mask |= MAY_READ;
    }
    if (flags & O_WRONLY)
    {
        mask |= MAY_WRITE;
    }
    if (flags & O_CREAT)
    {
        mask |= MAY_WRITE;
    }
    if (flags & O_RDWR)
    {
        mask |= MAY_READ | MAY_WRITE;
    }
    if (flags & O_APPEND)
    {
        mask |= MAY_APPEND;
    }
    if (flags & O_EXEC)
    {
        mask |= MAY_EXEC;
    }

    return mask;
}

ssize_t check_file_permission(struct process *proc, struct file *f,
                              perm_mask_t mask)
{
    bool needs_write_access = (mask & MAY_WRITE) || (mask & MAY_APPEND);
    bool needs_read_access = (mask & MAY_READ);

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
                               perm_mask_t mask)
{
    if (mask & MAY_UNLINK)
    {
        if (ip->i_mode & S_ISVTX)
        {
            // only owner or superuser can delete
            ASSERT_IS_FILE_OWNER(proc, ip);
            return 0;
        }
        else
        {
            // without sticky bit handle unlink as write permission to the
            // directory
            mask = mask | MAY_WRITE;
        }
    }

    if (mask & MAY_READ)
    {
        if (!permission_check_read(&proc->cred, ip->i_mode, ip->uid, ip->gid))
        {
            return -EACCES;
        }
    }
    if ((mask & MAY_WRITE) || (mask & MAY_APPEND))
    {
        if (!permission_check_write(&proc->cred, ip->i_mode, ip->uid, ip->gid))
        {
            return -EACCES;
        }
    }
    if (mask & MAY_EXEC)
    {
        if (!permission_check_execute(&proc->cred, ip->i_mode, ip->uid,
                                      ip->gid))
        {
            return -EACCES;
        }
    }

    return 0;
}