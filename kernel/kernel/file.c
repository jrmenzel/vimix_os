/* SPDX-License-Identifier: MIT */

//
// Support functions for system calls that involve file descriptors.
//

#include <drivers/block_device.h>
#include <drivers/character_device.h>
#include <ipc/pipe.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
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
    struct file *f = kmalloc(sizeof(struct file));
    if (f)
    {
        f->ref = 1;
        list_add(&f->list, &g_file_table.open_files);
    }
    spin_unlock(&g_file_table.lock);
    return f;
}

struct file *file_dup(struct file *f)
{
    spin_lock(&g_file_table.lock);
    DEBUG_EXTRA_ASSERT(f->ref >= 1,
                       "file_dup() called for a file with ref count 0");
    f->ref++;
    spin_unlock(&g_file_table.lock);
    return f;
}

FILE_DESCRIPTOR file_open(char *pathname, int32_t flags, mode_t mode)
{
    char name[NAME_MAX];
    struct inode *iparent = inode_of_parent_from_path(pathname, name);
    if (iparent == NULL)
    {
        return -ENOENT;
    }

    struct inode *ip = VFS_INODE_OPEN(iparent, name, flags);

    if (ip == NULL)
    {
        if (flags & O_CREAT)
        {
            // only create regular files this way:
            if (!check_and_adjust_mode(&mode, S_IFREG) || !S_ISREG(mode))
            {
                inode_put(iparent);
                return -EPERM;
            }

            ip = VFS_INODE_CREATE(iparent, name, mode, flags, iparent->dev);
            inode_put(iparent);
            // inode is locked if not NULL

            if (ip == NULL)
            {
                return -ENOENT;
            }
        }
        else
        {
            // file not found
            inode_put(iparent);
            return -ENOENT;
        }
    }
    else
    {
        inode_put(iparent);
        if (ip->is_mounted_on != NULL)
        {
            struct inode *tmp_ip = VFS_INODE_DUP(ip->is_mounted_on->s_root);
            inode_unlock_put(ip);
            inode_lock(tmp_ip);
            ip = tmp_ip;
        }
    }

    if (S_ISDIR(ip->i_mode) && flags != O_RDONLY)
    {
        inode_unlock_put(ip);
        return -EACCES;
    }

    if ((S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)) &&
        (MAJOR(ip->dev) < 0 || MINOR(ip->dev) >= MAX_DEVICES))
    {
        printk(
            "Kernel error: can't open device with invalid device number %d "
            "%d\n",
            MAJOR(ip->dev), MINOR(ip->dev));
        inode_unlock_put(ip);
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
        return -ENOMEM;
    }

    f->off = 0;
    f->mode = ip->i_mode;
    f->ip = ip;
    f->flags = flags;

    inode_unlock(ip);

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

    // that was the last reference -> close the file by removing it from the
    // open file list
    list_del(&f->list);
    spin_unlock(&g_file_table.lock);

    if (S_ISFIFO(f->mode))
    {
        bool close_writing_end = (f->flags & O_WRONLY) || (f->flags & O_RDWR);
        pipe_close(f->pipe, close_writing_end);
    }
    else if (S_ISCHR(f->mode) || S_ISBLK(f->mode) || S_ISDIR(f->mode) ||
             S_ISREG(f->mode))
    {
        inode_put(f->ip);
    }

    // free memory of file struct
    kfree((void *)f);
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
        read_bytes = cdev->ops.read(&cdev->dev, true, addr, n, f->off);
        f->off += read_bytes;
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
        ret = VFS_FILE_WRITE(f, addr, n);
    }
    else
    {
        panic("file_write() on unknown file type");
    }

    return ret;
}

ssize_t file_link(char *path_from, char *path_to)
{
    struct inode *ip = inode_from_path(path_from);
    if (ip == NULL)
    {
        return -ENOENT;
    }

    inode_lock(ip);
    if (S_ISDIR(ip->i_mode))
    {
        inode_unlock_put(ip);
        return -EISDIR;
    }
    inode_unlock(ip);

    char name[NAME_MAX];
    struct inode *dir = inode_of_parent_from_path(path_to, name);
    if (dir == NULL)
    {
        inode_put(ip);
        return -ENOENT;
    }

    inode_lock(dir);
    inode_lock(ip);
    if (dir->dev != ip->dev)
    {
        inode_unlock_put(ip);
        inode_unlock_put(dir);
        return -EOTHER;
    }

    return VFS_INODE_LINK(dir, ip, name);
}

ssize_t file_unlink(char *path, bool delete_files, bool delete_directories)
{
    char name[NAME_MAX];
    struct inode *dir = inode_of_parent_from_path(path, name);
    if (dir == NULL)
    {
        return -ENOENT;
    }

    // Cannot unlink "." or "..".
    if (file_name_cmp(name, ".") == 0 || file_name_cmp(name, "..") == 0)
    {
        inode_put(dir);
        return -EPERM;
    }

    return VFS_INODE_UNLINK(dir, name, delete_files, delete_directories);
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
