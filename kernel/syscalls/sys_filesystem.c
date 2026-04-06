/* SPDX-License-Identifier: MIT */

#include <fs/fs_lookup.h>
#include <kernel/mount.h>
#include <kernel/statvfs.h>
#include <syscalls/syscall.h>

syserr_t do_statvfs(struct super_block *sb, size_t buf_addr);

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
    struct dentry *dp = dentry_from_path(path, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dp))
    {
        dentry_put(dp);
        return -ENOENT;
    }

    syserr_t ret = do_statvfs(dp->ip->i_sb, buf_addr);
    dentry_put(dp);
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

    return do_statvfs(f->dp->ip->i_sb, buf_addr);
}

syserr_t sys_mount()
{
    size_t idx = 0;
    // parameter 0: const char *source
    char source[PATH_MAX];
    if (argstr(idx++, source, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 1: const char *target
    char target[PATH_MAX];
    if (argstr(idx++, target, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    // parameter 2: const char *filesystemtype
    char filesystemtype[64];
    if (argstr(idx++, filesystemtype, 64) < 0)
    {
        return -EFAULT;
    }

    // parameter 3: unsigned long mountflags
    size_t mountflags;
    argaddr(idx++, &mountflags);

    // parameter 4: const void *data
    size_t addr_data;
    argaddr(idx++, &addr_data);

    return do_mount(source, target, filesystemtype, (unsigned long)mountflags,
                    addr_data);
}

syserr_t sys_umount()
{
    // parameter 0: const char *target
    char target[PATH_MAX];
    if (argstr(0, target, PATH_MAX) < 0)
    {
        return -EFAULT;
    }

    return do_umount(target);
}

syserr_t do_statvfs(struct super_block *sb, size_t buf_addr)
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
