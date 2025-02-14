/* SPDX-License-Identifier: MIT */

//
// Device management system calls.
//

#include <drivers/character_device.h>
#include <kernel/file.h>
#include <kernel/kernel.h>
#include <kernel/stat.h>
#include <syscalls/syscall.h>

ssize_t sys_ioctl()
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

    if (S_ISCHR(f->ip->i_mode) == false)
    {
        // not a char device!
        return -ENODEV;
    }

    // parameter 2:
    size_t ip;
    argaddr(2, &ip);

    struct Character_Device *cdev = get_character_device(f->ip->dev);
    if (cdev == NULL)
    {
        return -ENODEV;
    }
    if (cdev->ops.ioctl == NULL)
    {
        return -ENOTTY;
    }
    return cdev->ops.ioctl(f->ip, request, (void *)ip);
}
