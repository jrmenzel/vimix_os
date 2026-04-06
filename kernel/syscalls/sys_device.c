/* SPDX-License-Identifier: MIT */

//
// Device management system calls.
//

#include <drivers/character_device.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/permission.h>
#include <kernel/stat.h>
#include <syscalls/syscall.h>

syserr_t do_ioctl(struct file *f, int request, void *optional_param);

syserr_t sys_ioctl()
{
    // parameter 0: int fd
    struct file *f;
    int fd;
    if (argfd(0, &fd, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: int request
    int request;
    if (argint(1, &request) < 0)
    {
        return -EINVAL;
    }

    // parameter 2:
    size_t optional_param;
    argaddr(2, &optional_param);

    return do_ioctl(f, request, (void *)optional_param);
}

syserr_t do_ioctl(struct file *f, int request, void *optional_param)
{
    struct inode *ip = f->dp->ip;
    if (S_ISCHR(ip->i_mode) == false)
    {
        // not a char device!
        return -ENODEV;
    }

    syserr_t perm = check_file_permission(get_current(), f, MAY_READ);
    if (perm < 0)
    {
        return perm;
    }

    struct Character_Device *cdev = get_character_device(ip->dev);
    if (cdev == NULL)
    {
        return -ENODEV;
    }
    if (cdev->ops.ioctl == NULL)
    {
        return -ENOTTY;
    }

    inode_lock_exclusive(ip);
    syserr_t res = cdev->ops.ioctl(ip, request, optional_param);
    inode_unlock_exclusive(ip);

    return res;
}
