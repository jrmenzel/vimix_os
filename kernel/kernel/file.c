/* SPDX-License-Identifier: MIT */

//
// Support functions for system calls that involve file descriptors.
//

#include <drivers/block_device.h>
#include <drivers/character_device.h>
#include <fs/xv6fs/log.h>
#include <ipc/pipe.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/sleeplock.h>
#include <kernel/spinlock.h>
#include <kernel/stat.h>
#include <kernel/string.h>
#include <kernel/unistd.h>

struct
{
    struct spinlock lock;
    struct file file[MAX_FILES_SUPPORTED];
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

void file_init() { spin_lock_init(&g_file_table.lock, "ftable"); }

struct file *file_alloc()
{
    spin_lock(&g_file_table.lock);
    for (struct file *f = g_file_table.file;
         f < (g_file_table.file + MAX_FILES_SUPPORTED); f++)
    {
        if (f->ref == 0)
        {
            f->ref = 1;
            spin_unlock(&g_file_table.lock);
            return f;
        }
    }
    spin_unlock(&g_file_table.lock);
    return NULL;
}

struct file *file_dup(struct file *f)
{
    spin_lock(&g_file_table.lock);
    if (f->ref < 1)
    {
        panic("file_dup() called for a file with ref count 0");
    }
    f->ref++;
    spin_unlock(&g_file_table.lock);
    return f;
}

FILE_DESCRIPTOR file_open_or_create(char *pathname, int32_t flags, mode_t mode)
{
    log_begin_fs_transaction();
    // Only getting the inode differs between opening an existing file or
    // creating one.
    struct inode *ip = NULL;

    if (flags & O_CREAT)
    {
        // only create regular files this way:
        if (!check_and_adjust_mode(&mode, S_IFREG) || !S_ISREG(mode))
        {
            log_end_fs_transaction();
            return -EPERM;
        }

        char name[NAME_MAX];
        struct inode *iparent = inode_of_parent_from_path(pathname, name);
        if (iparent != NULL)
        {
            inode_unlock(iparent);
            ip = inode_open_or_create(pathname, mode, iparent->dev);
            inode_put(iparent);
        }

        if (ip == NULL)
        {
            log_end_fs_transaction();
            return -ENOENT;
        }
    }
    else
    {
        ip = inode_from_path(pathname);
        if (ip == NULL)
        {
            log_end_fs_transaction();
            return -ENOENT;
        }
        inode_lock(ip);
        if (S_ISDIR(ip->i_mode) && flags != O_RDONLY)
        {
            inode_unlock_put(ip);
            log_end_fs_transaction();
            return -EACCES;
        }
    }

    if ((S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)) &&
        (MAJOR(ip->dev) < 0 || MINOR(ip->dev) >= MAX_DEVICES))
    {
        printk(
            "Kernel error: can't open device with invalid device number %d "
            "%d\n",
            MAJOR(ip->dev), MINOR(ip->dev));
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -ENODEV;
    }

    struct file *f;
    FILE_DESCRIPTOR fd;
    if ((f = file_alloc()) == NULL || (fd = fd_alloc(f)) < 0)
    {
        if (f)
        {
            file_close(f);
        }
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -ENOMEM;
    }

    if (INODE_HAS_TYPE(ip->i_mode) == false)
    {
        panic("inode has no type");
    }

    f->off = 0;
    f->mode = ip->i_mode;
    f->ip = ip;
    f->flags = flags;

    if (INODE_HAS_TYPE(ip->i_mode) == false)
    {
        panic("inode has no type");
    }

    if ((flags & O_TRUNC) && S_ISREG(ip->i_mode))
    {
        inode_trunc(ip);
    }

    inode_unlock(ip);
    log_end_fs_transaction();

    return fd;
}

void file_close(struct file *f)
{
    spin_lock(&g_file_table.lock);
    if (f->ref < 1)
    {
        panic("file_close() called for file without open references");
    }

    f->ref--;

    if (f->ref > 0)
    {
        spin_unlock(&g_file_table.lock);
        return;
    }

    // that was the last reference -> close the file
    // make a copy of the struct to savely access the values after
    // unlocking the file table access lock.
    struct file ff = *f;
    f->ref = 0;
    f->mode = 0;
    spin_unlock(&g_file_table.lock);

    if (S_ISFIFO(ff.mode))
    {
        bool close_writing_end = (ff.flags & O_WRONLY) || (ff.flags & O_RDWR);
        pipe_close(ff.pipe, close_writing_end);
    }
    else if (S_ISCHR(ff.mode) || S_ISBLK(ff.mode) || S_ISDIR(ff.mode) ||
             S_ISREG(ff.mode))
    {
        log_begin_fs_transaction();
        inode_put(ff.ip);
        log_end_fs_transaction();
    }
}

int32_t file_stat(struct file *f, size_t addr)
{
    if (INODE_HAS_TYPE(f->mode) == false)
    {
        panic("file_stat(): inode has no type");
    }

    if (S_ISREG(f->mode) || S_ISDIR(f->mode) || S_ISCHR(f->mode) ||
        S_ISBLK(f->mode))
    {
        struct stat st;
        struct process *proc = get_current();

        inode_lock(f->ip);
        inode_stat(f->ip, &st);
        inode_unlock(f->ip);
        if (uvm_copy_out(proc->pagetable, addr, (char *)&st, sizeof(st)) < 0)
        {
            return -EFAULT;
        }
        return 0;
    }
    return -EBADF;
}

ssize_t file_read(struct file *f, size_t addr, size_t n)
{
    ssize_t read_bytes = 0;

    if (f->flags & O_WRONLY)
    {
        return -EACCES;
    }

    if (S_ISFIFO(f->mode))
    {
        read_bytes = pipe_read(f->pipe, addr, n);
    }
    else if (S_ISCHR(f->mode))
    {
        struct Character_Device *cdev = get_character_device(f->ip->dev);
        if (cdev == NULL)
        {
            return -ENODEV;
        }
        read_bytes = cdev->ops.read(&cdev->dev, true, addr, n);
    }
    else if (S_ISBLK(f->mode))
    {
        struct Block_Device *bdev = get_block_device(f->ip->dev);
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
    else if (S_ISREG(f->mode) || S_ISDIR(f->mode))
    {
        inode_lock(f->ip);
        read_bytes = inode_read(f->ip, true, addr, f->off, n);
        if (read_bytes > 0)
        {
            // check > 0 is needed, read_bytes can be negative on error
            f->off += read_bytes;
        }
        inode_unlock(f->ip);
    }
    else
    {
        panic("file_read() on unknown file type");
    }

    return read_bytes;
}

ssize_t file_write(struct file *f, size_t addr, size_t n)
{
    ssize_t ret = 0;

    if (!((f->flags & O_WRONLY) || (f->flags & O_RDWR)))
    {
        return -EACCES;
    }

    if (S_ISFIFO(f->mode))
    {
        ret = pipe_write(f->pipe, addr, n);
    }
    else if (S_ISCHR(f->mode))
    {
        struct Character_Device *cdev = get_character_device(f->ip->dev);
        if (cdev == NULL)
        {
            return -ENODEV;
        }
        ret = cdev->ops.write(&cdev->dev, true, addr, n);
        if (ret > 0) f->off += ret;
    }
    else if (S_ISBLK(f->mode))
    {
        struct Block_Device *bdev = get_block_device(f->ip->dev);
        if (bdev == NULL)
        {
            return -ENODEV;
        }
        ret = block_device_write(bdev, addr, f->off, n);
        if (ret > 0) f->off += ret;
    }
    else if (S_ISREG(f->mode) || S_ISDIR(f->mode))
    {
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since inode_write()
        // might be writing a device like the console.
        ssize_t max = ((MAX_OP_BLOCKS - 1 - 1 - 2) / 2) * BLOCK_SIZE;
        ssize_t i = 0;
        while (i < n)
        {
            ssize_t r;
            ssize_t n1 = n - i;
            if (n1 > max)
            {
                n1 = max;
            }

            log_begin_fs_transaction();
            inode_lock(f->ip);

            r = inode_write(f->ip, true, addr + i, f->off, n1);
            if (r > 0)
            {
                f->off += r;
            }
            inode_unlock(f->ip);
            log_end_fs_transaction();

            if (r != n1)
            {
                // error from inode_write
                break;
            }
            i += r;
        }
        ret = (i == n ? n : -1);
    }
    else
    {
        panic("file_write() on unknown file type");
    }

    return ret;
}

ssize_t file_link(char *path_from, char *path_to)
{
    log_begin_fs_transaction();
    struct inode *ip = inode_from_path(path_from);
    if (ip == NULL)
    {
        log_end_fs_transaction();
        return -ENOENT;
    }

    inode_lock(ip);
    if (S_ISDIR(ip->i_mode))
    {
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -EISDIR;
    }

    ip->nlink++;
    inode_update(ip);
    inode_unlock(ip);

    char name[NAME_MAX];
    struct inode *dir = inode_of_parent_from_path(path_to, name);
    if (dir == NULL)
    {
        inode_lock(ip);
        ip->nlink--;
        inode_update(ip);
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -ENOENT;
    }

    if (dir->dev != ip->dev || inode_dir_link(dir, name, ip->inum) < 0)
    {
        inode_unlock_put(dir);

        inode_lock(ip);
        ip->nlink--;
        inode_update(ip);
        inode_unlock_put(ip);
        log_end_fs_transaction();
        return -EOTHER;
    }
    inode_unlock_put(dir);
    inode_put(ip);

    log_end_fs_transaction();

    return 0;
}

/// Is the directory dir empty except for "." and ".." ?
static int isdirempty(struct inode *dir)
{
    struct xv6fs_dirent de;

    for (size_t off = 2 * sizeof(de); off < dir->size; off += sizeof(de))
    {
        if (inode_read(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
        {
            panic("isdirempty: inode_read");
        }

        if (de.inum != 0)
        {
            return 0;
        }
    }
    return 1;
}

ssize_t file_unlink(char *path, bool delete_files, bool delete_directories)
{
    log_begin_fs_transaction();
    char name[NAME_MAX];
    struct inode *dir = inode_of_parent_from_path(path, name);
    if (dir == NULL)
    {
        log_end_fs_transaction();
        return -ENOENT;
    }

    // Cannot unlink "." or "..".
    if (file_name_cmp(name, ".") == 0 || file_name_cmp(name, "..") == 0)
    {
        inode_unlock_put(dir);
        log_end_fs_transaction();
        return -EPERM;
    }

    uint32_t off;
    struct inode *ip = inode_dir_lookup(dir, name, &off);
    if (ip == NULL)
    {
        inode_unlock_put(dir);
        log_end_fs_transaction();
        return -ENOENT;
    }
    inode_lock(ip);

    if (ip->nlink < 1)
    {
        panic("unlink: nlink < 1");
    }

    ssize_t error = 0;
    if (S_ISDIR(ip->i_mode) && (!delete_directories))
    {
        error = -EISDIR;
    }
    if (!S_ISDIR(ip->i_mode) && (!delete_files))
    {
        error = -ENOTDIR;
    }
    if (S_ISDIR(ip->i_mode) && !isdirempty(ip))
    {
        error = -ENOTEMPTY;
    }

    if (error != 0)
    {
        inode_unlock_put(ip);
        inode_unlock_put(dir);
        log_end_fs_transaction();
        return error;
    }

    // delete directory entry by over-writing it with zeros:
    struct xv6fs_dirent de;
    memset(&de, 0, sizeof(de));
    if (inode_write(dir, false, (size_t)&de, off, sizeof(de)) != sizeof(de))
    {
        panic("unlink: inode_write");
    }

    if (S_ISDIR(ip->i_mode))
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
}

ssize_t file_lseek(struct file *f, ssize_t offset, int whence)
{
    if (!S_ISREG(f->mode) && !S_ISBLK(f->mode))
    {
        return -ESPIPE;  // only correct error for pipes...
    }

    size_t file_size = f->ip->size;
    if (S_ISBLK(f->mode))
    {
        struct Block_Device *bdevice = get_block_device(f->ip->dev);
        if (!bdevice)
        {
            panic("file_lseek: bad device");
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

    return f->off;
}
