/* SPDX-License-Identifier: MIT */

#include <kernel/statvfs.h>
#include <syscalls/syscall.h>

syserr_t statvfs_internal(struct super_block *sb, size_t buf_addr)
{
    struct statvfs buf;
    VFS_SUPER_STATVFS(sb, &buf);

    if (uvm_copy_out(get_current()->pagetable, buf_addr, (char *)&buf,
                     sizeof(buf)) < 0)
    {
        return -EFAULT;
    }
    return 0;
}

syserr_t sys_statvfs()
{
    // parameter 0: const char *path
    char path[PATH_MAX];
    if (argstr(0, path, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: struct statvfs *buf
    size_t buf_addr;
    argaddr(1, &buf_addr);

    syserr_t error = 0;
    struct inode *ip = inode_from_path(path, &error);
    if (ip == NULL)
    {
        return error;
    }
    syserr_t ret = statvfs_internal(ip->i_sb, buf_addr);
    inode_put(ip);
    return ret;
}

syserr_t sys_fstatvfs()
{
    // parameter 0: int fd
    struct file *f;
    if (argfd(0, NULL, &f) < 0)
    {
        return -EBADF;
    }

    // parameter 1: struct statvfs *buf
    size_t buf_addr;
    argaddr(1, &buf_addr);

    return statvfs_internal(f->ip->i_sb, buf_addr);
}