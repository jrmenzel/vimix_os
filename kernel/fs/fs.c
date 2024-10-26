/* SPDX-License-Identifier: MIT */

// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sys_file.c.

#include <fs/vfs.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/bio.h>
#include <kernel/buf.h>
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
#include <lib/minmax.h>

dev_t ROOT_DEVICE_NUMBER;
struct super_block *ROOT_SUPER_BLOCK = NULL;

struct super_block g_active_file_systems[MAX_MOUNTED_FILE_SYSTEMS] = {0};

struct super_block *get_free_super_block()
{
    for (size_t i = 0; i < MAX_MOUNTED_FILE_SYSTEMS; ++i)
    {
        if (g_active_file_systems[i].dev == INVALID_DEVICE)
        {
            return &g_active_file_systems[i];
        }
    }
    return NULL;
}

int mount_types(struct inode *source, dev_t target,
                struct file_system_type *filesystemtype,
                unsigned long mountflags, const void *data)
{
    struct super_block *sb = get_free_super_block();
    sb->dev = target;

    if (filesystemtype->fill_super_block(sb, data) != 0)
    {
        return -1;
    }

    if (source == NULL)
    {
        // source == NULL means this is the root file system, so it's legal
        ROOT_SUPER_BLOCK = sb;
        sb->imounted_on = NULL;
    }
    else
    {
        source->is_mounted_on = sb;
        sb->imounted_on = xv6fs_iops_dup(source);
    }
    sb->s_mountflags = mountflags;
    sb->s_root = xv6fs_iops_iget_root(sb);

    return 0;
}

void init_root_file_system(dev_t dev, const char *fs_name)
{
    struct file_system_type **file_system =
        find_filesystem(fs_name, strlen(fs_name));

    if (*file_system == NULL)
    {
        printk("no support for file system %s\n", fs_name);
        panic("root file system init failed");
    }

    unsigned long flags = 0;
    int m = mount_types(NULL, dev, *file_system, flags, NULL);
    if (m != 0)
    {
        panic("root file system init failed, could not mount /");
    }

    printk("root file system init done\n");
}

ssize_t inode_open_or_create2(const char *pathname, mode_t mode, dev_t device)
{
    char name[NAME_MAX];
    struct inode *dir = inode_of_parent_from_path(pathname, name);
    if (dir == NULL)
    {
        return -ENOENT;
    }

    struct inode *ip = xv6fs_iops_open_create(dir, name, mode, 0, device);
    if (ip == NULL)
    {
        return -ENOENT;
    }

    inode_unlock_put(ip);
    return 0;
}

void inode_lock(struct inode *ip)
{
    if (ip == NULL)
    {
        panic("inode_lock: inode is NULL");
    }
    if (ip->ref < 1)
    {
        panic("inode_lock: inode has an invalid reference count");
    }

    sleep_lock(&ip->lock);

    if (ip->valid == 0)
    {
        xv6fs_iops_read_in(ip);
        ip->valid = 1;
        if (INODE_HAS_TYPE(ip->i_mode) == false)
        {
            panic("inode_lock: inode has no type");
        }
    }
}

void inode_unlock(struct inode *ip)
{
#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
    if (ip == NULL)
    {
        panic("inode_unlock failed: inode is NULL");
    }
    if (ip->ref < 1)
    {
        panic("inode_unlock failed: reference count invalid");
    }
#endif  // CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
#ifdef CONFIG_DEBUG_SLEEPLOCK
    if (!sleep_lock_is_held_by_this_cpu(&ip->lock))
    {
        panic("inode_unlock failed: sleeplock not held by this CPU");
    }
#endif  // CONFIG_DEBUG_SLEEPLOCK

    sleep_unlock(&ip->lock);
}

void inode_put(struct inode *ip) { xv6fs_iops_put(ip, true); }

void inode_unlock_put(struct inode *ip)
{
    inode_unlock(ip);
    inode_put(ip);
}

void inode_stat(struct inode *ip, struct stat *st)
{
    st->st_dev = ip->i_sb->dev;
    st->st_rdev = ip->dev;
    st->st_ino = ip->inum;
    st->st_mode = ip->i_mode;
    st->st_nlink = ip->nlink;
    st->st_size = ip->size;
    st->st_blksize = BLOCK_SIZE;
    st->st_blocks = (ip->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

ssize_t inode_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                   size_t off, size_t n)
{
    if (off > ip->size || off + n < off)
    {
        return 0;
    }
    if (off + n > ip->size)
    {
        n = ip->size - off;
    }

    return xv6fs_iops_read(ip, addr_is_userspace, dst, off, n);
}

// Directories

int file_name_cmp(const char *s, const char *t)
{
    return strncmp(s, t, NAME_MAX);
}

struct inode *inode_dir_lookup(struct inode *dir, const char *name)
{
    if (!S_ISDIR(dir->i_mode))
    {
        panic("inode_dir_lookup dir parameter is not a DIR!");
    }

    return xv6fs_iops_dir_lookup(dir, name, NULL);
}

int inode_dir_link(struct inode *dir, char *name, uint32_t inum)
{
    // Check that name is not present.
    struct inode *ip = inode_dir_lookup(dir, name);
    if (ip != NULL)
    {
        inode_put(ip);
        return -1;
    }

    return xv6fs_iops_dir_link(dir, name, inum);
}

// Paths

/// Copy the next path element from path into name.
/// Return a pointer to the element following the copied one.
/// The returned path has no leading slashes,
/// so the caller can check *path=='\0' to see if the name is the last one.
/// If no name to remove, return 0.
///
/// Examples:
///   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
///   skipelem("///a//bb", name) = "bb", setting name = "a"
///   skipelem("a", name) = "", setting name = "a"
///   skipelem("", name) = skipelem("////", name) = 0
static const char *skipelem(const char *path, char *name)
{
    while (*path == '/')
    {
        path++;
    }
    if (*path == 0)
    {
        return NULL;
    }

    const char *s = path;
    while (*path != '/' && *path != 0)
    {
        path++;
    }

    size_t len = path - s;
    if (len >= NAME_MAX)
    {
        memmove(name, s, NAME_MAX);
    }
    else
    {
        memmove(name, s, len);
        name[len] = 0;
    }

    while (*path == '/')
    {
        path++;
    }
    return path;
}

/// Look up and return the inode for a path name.
/// If get_parent == true, return the inode for the parent and copy the final
/// path element into name, which must have room for NAME_MAX bytes.
/// Must be called inside a transaction since it calls inode_put().
static struct inode *namex(const char *path, bool get_parent, char *name)
{
    struct inode *ip;

    if (*path == '/')
    {
        DEBUG_EXTRA_ASSERT(ROOT_SUPER_BLOCK != NULL, "No root filesystem!");
        ip = xv6fs_iops_iget_root(ROOT_SUPER_BLOCK);
    }
    else
    {
        ip = xv6fs_iops_dup(get_current()->cwd);
    }

    while ((path = skipelem(path, name)) != 0)
    {
        inode_lock(ip);

        if (INODE_HAS_TYPE(ip->i_mode) == false)
        {
            panic("inode_lock: inode has no type");
        }

        if (!S_ISDIR(ip->i_mode))
        {
            inode_unlock_put(ip);
            return NULL;
        }
        if (get_parent && *path == '\0')
        {
            // Stop one level early, return parent ip
            inode_unlock(ip);
            return ip;
        }
        struct inode *next = NULL;

        next = inode_dir_lookup(ip, name);
        if (next == NULL)
        {
            inode_unlock_put(ip);
            return NULL;
        }
        if (next->is_mounted_on)
        {
            struct inode *tmp = xv6fs_iops_dup(next->is_mounted_on->s_root);
            inode_put(next);
            next = tmp;
        }

        inode_unlock_put(ip);
        ip = next;
    }
    if (get_parent)
    {
        inode_put(ip);
        return NULL;
    }
    return ip;  // return ip
}

struct inode *inode_from_path(const char *path)
{
    char name[NAME_MAX];
    return namex(path, false, name);
}

struct inode *inode_of_parent_from_path(const char *path, char *name)
{
    return namex(path, true, name);
}

void debug_print_inode(struct inode *ip)
{
    printk("inode %d on (%d,%d), ", ip->inum, MAJOR(ip->i_sb->dev),
           MINOR(ip->i_sb->dev));
    printk("ref: %d, ", ip->ref);
    if (ip->valid)
    {
        printk("link: %d, ", ip->nlink);

        if (S_ISREG(ip->i_mode))
        {
            printk("regular file");
        }
        else if (S_ISDIR(ip->i_mode))
        {
            printk("directory");
        }
        else if (S_ISCHR(ip->i_mode))
        {
            printk("char dev (%d,%d)", MAJOR(ip->dev), MINOR(ip->dev));
        }
        else if (S_ISBLK(ip->i_mode))
        {
            printk("block dev (%d,%d)", MAJOR(ip->dev), MINOR(ip->dev));
        }
        else if (S_ISFIFO(ip->i_mode))
        {
            printk("pipe");
        }
    }
    else
    {
        printk("inode not read from disk");
    }
    if (ip->lock.locked)
    {
        printk(" LOCKED (0x%zx)", (size_t)&ip->lock);
    }
#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    printk(" - %s", ip->path);
#endif
}

void debug_print_inodes() { xv6fs_debug_print_inodes(); }
