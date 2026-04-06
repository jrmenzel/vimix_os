/* SPDX-License-Identifier: MIT */

//
// Support functions for system calls that involve file descriptors.
//

#include <drivers/block_device.h>
#include <drivers/character_device.h>
#include <drivers/rtc.h>
#include <fs/fs_lookup.h>
#include <ipc/pipe.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/permission.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <kernel/unistd.h>
#include <mm/kalloc.h>

struct
{
    struct spinlock lock;         ///< global lock for all open files
    struct list_head open_files;  ///< double linked list of open files
} g_file_table;

bool check_and_adjust_mode(mode_t *mode, mode_t default_type)
{
    size_t types = 0;
    if (S_ISREG(*mode)) types++;
    if (S_ISDIR(*mode)) types++;
    if (S_ISCHR(*mode)) types++;
    if (S_ISBLK(*mode)) types++;
    if (S_ISFIFO(*mode)) types++;
    if (types > 1)
    {
        printk("ERROR: file type %x claims to be multiple types\n", *mode);
        return false;
    }

    // default type if no file type was given
    if ((*mode & S_IFMT) == 0)
    {
        *mode |= default_type;
    }

    return true;
}

void file_init()
{
    spin_lock_init(&g_file_table.lock, "ftable");
    list_init(&g_file_table.open_files);
}

struct file *file_alloc()
{
    spin_lock(&g_file_table.lock);
    struct file *f = kmalloc(sizeof(struct file), ALLOC_FLAG_ZERO_MEMORY);
    if (f)
    {
        kref_init(&f->ref);
        list_add(&f->list, &g_file_table.open_files);
    }
    spin_unlock(&g_file_table.lock);
    return f;
}

struct file *file_alloc_init(mode_t mode, int32_t flags, struct dentry *dp)
{
    struct file *f = file_alloc();
    if (f == NULL)
    {
        return NULL;
    }

    f->off = 0;
    f->mode = mode;
    f->dp = dentry_get(dp);
    f->flags = flags;

    return f;
}

struct file *file_get(struct file *f)
{
    DEBUG_EXTRA_ASSERT(kref_read(&f->ref) >= 1,
                       "file_get() called for a file with ref count 0");
    kref_get(&f->ref);
    return f;
}

syserr_t do_open(char *pathname, int32_t flags, mode_t mode)
{
    mode = mode & 0777;  // only permission bits

    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(pathname, &error);
    if (dp == NULL)
    {
        return error;
    }

    struct file *f = file_alloc_init(mode, flags, dp);
    if (f == NULL)
    {
        dentry_put(dp);
        return -ENOMEM;
    }

    struct process *proc = get_current();

    if (dentry_is_valid(dp))
    {
        if (S_ISDIR(dp->ip->i_mode) && flags != O_RDONLY)
        {
            dentry_put(dp);
            file_close(f);
            return -EACCES;
        }

        syserr_t perm_ok =
            check_dentry_permission(proc, dp, perm_mask_from_open_flags(flags));
        if (perm_ok < 0)
        {
            dentry_put(dp);
            file_close(f);
            return perm_ok;
        }

        error = VFS_FILE_OPEN(dp->ip, f);
    }
    else if (flags & O_CREAT)
    {
        //  file does not exist -> create it
        dentry_lock(dp);
        struct dentry *parent = dentry_get(dp->parent);
        dentry_unlock(dp);

        if (check_dentry_permission(proc, parent, MAY_WRITE) < 0)
        {
            dentry_put(parent);
            dentry_put(dp);
            file_close(f);
            return -EACCES;
        }

        // apply process umask
        mode &= ~(proc->umask);
        mode |= (mode & S_IFMT);

        struct inode *parent_ip = parent->ip;

        inode_lock_exclusive(parent_ip);
        if (dp->ip != NULL)
        {
            // created concurrently
            inode_unlock_exclusive(parent_ip);

            syserr_t perm_ok = check_dentry_permission(
                get_current(), dp, perm_mask_from_open_flags(flags));
            if (perm_ok < 0)
            {
                dentry_put(dp);
                dentry_put(parent);
                file_close(f);
                return perm_ok;
            }

            error = VFS_FILE_OPEN(dp->ip, f);
        }
        else
        {
            error = VFS_INODE_CREATE(parent_ip, dp, mode | S_IFREG, flags);
            inode_unlock_exclusive(parent_ip);
        }

        dentry_put(parent);
    }
    else
    {
        dentry_put(dp);
        file_close(f);
        return -ENOENT;
    }
    if (dp->ip != NULL)
    {
        f->mode = dp->ip->i_mode;
    }
    dentry_put(dp);

    if (error < 0)
    {
        file_close(f);
        return error;
    }
    FILE_DESCRIPTOR fd = fd_alloc(f);
    if (fd < 0)
    {
        file_close(f);
        return -EMFILE;
    }

    return (syserr_t)fd;
}

void file_close(struct file *f)
{
    DEBUG_EXTRA_PANIC(f != NULL, "file_close() on NULL");
    spin_lock(&g_file_table.lock);
    if (kref_read(&f->ref) < 1)
    {
        panic("file_close() called for file without open references");
    }

    kref_put(&f->ref);

    if (kref_read(&f->ref) > 0)
    {
        spin_unlock(&g_file_table.lock);
        return;
    }

    // that was the last reference -> close the file by removing it from the
    // open file list
    list_del(&f->list);
    spin_unlock(&g_file_table.lock);

    if (S_ISFIFO(f->mode))
    {
        bool close_writing_end = (f->flags == O_WRONLY) || (f->flags == O_RDWR);
        pipe_close(f->pipe, close_writing_end);
    }
    else
    {
        dentry_put(f->dp);
    }

    // free memory of file struct
    kfree((void *)f);
}

syserr_t do_read(struct file *f, size_t addr, size_t n)
{
    ssize_t read_bytes = 0;

    syserr_t perm_ok = check_file_permission(get_current(), f, MAY_READ);
    if (perm_ok < 0)
    {
        return perm_ok;
    }

    if (S_ISDIR(f->mode))
    {
        return -EISDIR;
    }
    else if (S_ISFIFO(f->mode))
    {
        read_bytes = pipe_read(f->pipe, addr, n);
    }
    else
    {
        // note: pipes don't have inodes or dentries

        struct inode *ip = f->dp->ip;
        if (S_ISCHR(f->mode))
        {
            struct Character_Device *cdev = get_character_device(ip->dev);
            if (cdev == NULL)
            {
                return -ENODEV;
            }
            read_bytes = cdev->ops.read(&cdev->dev, true, addr, n, f->off);
            f->off += read_bytes;
        }
        else if (S_ISBLK(f->mode))
        {
            struct Block_Device *bdev = get_block_device(ip->dev);
            if (bdev == NULL)
            {
                return -ENODEV;
            }

            read_bytes = block_device_read(bdev, addr, f->off, n);
            if (read_bytes > 0)
            {
                // check > 0 is needed, read_bytes can be negative on error
                f->off += read_bytes;
            }
        }
        else if (S_ISREG(f->mode))
        {
            read_bytes = VFS_FILE_READ(f, addr, n);
            if (read_bytes > 0)
            {
                // check > 0 is needed, read_bytes can be negative on error
                f->off += read_bytes;
            }
        }
        else
        {
            panic("do_read() on unknown file type");
        }
    }

    return (syserr_t)read_bytes;
}

void file_update_mtime(struct file *f)
{
    DEBUG_EXTRA_PANIC((S_ISREG(f->mode) || S_ISDIR(f->mode)),
                      "file_update_mtime() on non-regular file");

    struct timespec time = rtc_get_time();
    time_t now = time.tv_sec;
    inode_lock(f->dp->ip);
    f->dp->ip->mtime = now;
    inode_unlock(f->dp->ip);
}

syserr_t do_write(struct file *f, size_t addr, size_t n)
{
    syserr_t ret = 0;

    syserr_t perm_ok = check_file_permission(get_current(), f, MAY_WRITE);
    if (perm_ok < 0)
    {
        return perm_ok;
    }

    if (S_ISDIR(f->mode))
    {
        return -EISDIR;
    }
    else if (S_ISFIFO(f->mode))
    {
        ret = pipe_write(f->pipe, addr, n);
    }
    else
    {
        struct inode *ip = f->dp->ip;
        inode_lock_exclusive(ip);

        if (S_ISCHR(f->mode))
        {
            struct Character_Device *cdev = get_character_device(ip->dev);
            if (cdev == NULL)
            {
                inode_unlock_exclusive(ip);
                return -ENODEV;
            }
            ret = cdev->ops.write(&cdev->dev, true, addr, n);
            if (ret > 0) f->off += ret;
        }
        else if (S_ISBLK(f->mode))
        {
            struct Block_Device *bdev = get_block_device(ip->dev);
            if (bdev == NULL)
            {
                inode_unlock_exclusive(ip);
                return -ENODEV;
            }
            ret = block_device_write(bdev, addr, f->off, n);
            if (ret > 0) f->off += ret;
        }
        else if (S_ISREG(f->mode))
        {
            ret = VFS_FILE_WRITE(f, addr, n);
            if (ret > 0) file_update_mtime(f);
        }
        else
        {
            printk("do_write(): unknown file type %x\n", f->mode);
            panic("do_write() on unknown file type");
        }
        inode_unlock_exclusive(ip);
    }

    return ret;
}

syserr_t do_lseek(struct file *f, ssize_t offset, int whence)
{
    if (!S_ISREG(f->mode) && !S_ISBLK(f->mode))
    {
        return -ESPIPE;  // only correct error for pipes...
    }

    size_t file_size = f->dp->ip->size;
    if (S_ISBLK(f->mode))
    {
        struct Block_Device *bdevice = get_block_device(f->dp->ip->dev);
        if (!bdevice)
        {
            return -ENODEV;
        }
        file_size = bdevice->size;
    }

    ssize_t new_pos = 0;
    switch (whence)
    {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos = f->off + offset; break;
        case SEEK_END: new_pos = file_size + offset; break;
        default: return -EINVAL; break;
    }

    if (new_pos < 0) return -EINVAL;
    if (new_pos > file_size) return -EINVAL;  // todo: support

    f->off = new_pos;

    return (syserr_t)f->off;
}
