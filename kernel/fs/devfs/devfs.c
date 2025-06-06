/* SPDX-License-Identifier: MIT */

#include <drivers/device.h>
#include <fs/devfs/devfs.h>
#include <kernel/errno.h>
#include <kernel/param.h>
#include <kernel/proc.h>
#include <kernel/string.h>

struct file_system_type devfs_file_system_type;
const char *DEV_FS_NAME = "devfs";

// the dir itself + all possible devices:
#define DEVFS_RESERVED_INODES (1)
#define DEVFS_MAX_ACTIVE_INODES \
    (DEVFS_RESERVED_INODES + (MAX_DEVICES * MAX_MINOR_DEVICES))
struct
{
    struct spinlock lock;
    size_t used_inodes;
    struct inode inode[DEVFS_MAX_ACTIVE_INODES];
    const char *name[DEVFS_MAX_ACTIVE_INODES];
} devfs_itable;

#define DEVFS_INVALID_INODE_NUMBER (0xDEADF00D)

ssize_t devfs_init_fs_super_block(struct super_block *sb_in, const void *data);
void devfs_kill_sb(struct super_block *sb_in);
struct inode *devfs_iops_dir_lookup(struct inode *dir, const char *name,
                                    uint32_t *poff);

struct inode *devfs_sops_iget_root(struct super_block *sb)
{
    DEBUG_EXTRA_ASSERT(devfs_itable.inode[0].inum != DEVFS_INVALID_INODE_NUMBER,
                       "DEV FS not initialized");

    return &devfs_itable.inode[0];
}

struct inode *devfs_sops_alloc_inode(struct super_block *sb, mode_t mode)
{
    return NULL;  // no new inodes can get created
}

int devfs_sops_write_inode(struct inode *ip)
{
    printk("devfs_sops_write_inode\n");
    return 0;
}

// super block operations
struct super_operations devfs_s_op = {
    iget_root : devfs_sops_iget_root,
    alloc_inode : devfs_sops_alloc_inode,
    write_inode : devfs_sops_write_inode
};

struct inode *devfs_iops_create(struct inode *iparent, char name[NAME_MAX],
                                mode_t mode, int32_t flags, dev_t device)
{
    return NULL;  // no new inodes can get created
}

struct inode *devfs_iops_open(struct inode *iparent, char name[NAME_MAX],
                              int32_t flags)
{
    inode_lock(iparent);
    struct inode *ip = devfs_iops_dir_lookup(iparent, name, NULL);
    inode_unlock(iparent);
    if (ip == NULL)
    {
        // file not found
        return NULL;
    }
    inode_lock(ip);

#if defined(CONFIG_DEBUG_INODE_PATH_NAME)
    strncpy(ip->path, name, PATH_MAX);
#endif
    // printk("devfs_iops_open %s from %d\n", name, iparent->inum);
    return ip;  // return locked
}

void devfs_iops_read_in(struct inode *ip)
{
    // all inodes are defined statically at init, nothing to do here
}

struct inode *devfs_iops_dup(struct inode *ip)
{
    spin_lock(&devfs_itable.lock);
    ip->ref++;
    spin_unlock(&devfs_itable.lock);
    return ip;
}

void devfs_iops_put(struct inode *ip)
{
    spin_lock(&devfs_itable.lock);
    DEBUG_EXTRA_ASSERT(ip->ref > 0,
                       "Can't put an inode that is not held by anyone");
    ip->ref--;
    spin_unlock(&devfs_itable.lock);

    // not backed by a device, so no write back for dev fs
}

struct inode *devfs_iops_dir_lookup(struct inode *dir, const char *name,
                                    uint32_t *poff)
{
    if (!S_ISDIR(dir->i_mode)) return NULL;

    if (strcmp(name, ".") == 0)
    {
        if (poff) *poff = 0;
        return devfs_iops_dup(dir);
    }
    if (strcmp(name, "..") == 0)
    {
        if (poff) *poff = 1;
        // return VFS_INODE_DUP(dir->i_sb->imounted_on);
        inode_lock(dir->i_sb->imounted_on);
        struct inode *ret =
            VFS_INODE_DIR_LOOKUP(dir->i_sb->imounted_on, "..", NULL);
        inode_unlock(dir->i_sb->imounted_on);
        return ret;
    }

    for (size_t i = 0; i < DEVFS_MAX_ACTIVE_INODES; ++i)
    {
        if ((devfs_itable.name[i] != NULL) &&
            (strcmp(name, devfs_itable.name[i]) == 0))
        {
            return devfs_iops_dup(&devfs_itable.inode[i]);
        }
    }

    return NULL;  // not found
}

int devfs_iops_dir_link(struct inode *dir, char *name, uint32_t inum)
{
    printk("devfs_iops_dir_link\n");
    return 0;
}

ssize_t devfs_iops_get_dirent(struct inode *dir, size_t dir_entry_addr,
                              bool addr_is_userspace, ssize_t seek_pos)
{
    if (!S_ISDIR(dir->i_mode) || seek_pos < 0) return -1;

    if (seek_pos > devfs_itable.used_inodes) return 0;

    struct dirent dir_entry;
    dir_entry.d_off = seek_pos + 1;
    dir_entry.d_reclen = sizeof(struct dirent);

    if (seek_pos == 0)
    {
        // "."
        dir_entry.d_ino = dir->inum;
        strncpy(dir_entry.d_name, ".", MAX_DIRENT_NAME);
    }
    else if (seek_pos == 1)
    {
        // ".."
        dir_entry.d_ino = dir->i_sb->imounted_on->inum;
        strncpy(dir_entry.d_name, "..", MAX_DIRENT_NAME);
    }
    else
    {
        // inode 0 is root, seek_pos 2 is the first device -> inode 1
        size_t device_index = seek_pos - 1;
        // struct inode *ip = &devfs_itable.inode[device_index];
        dir_entry.d_ino = device_index;
        strncpy(dir_entry.d_name, devfs_itable.name[device_index],
                MAX_DIRENT_NAME);
    }

    int32_t res = either_copyout(addr_is_userspace, dir_entry_addr,
                                 (void *)&dir_entry, sizeof(struct dirent));
    if (res < 0) return -EFAULT;

    // printk("devfs_iops_get_dirent %s\n", dir_entry.d_name);
    return seek_pos + 1;
}

ssize_t devfs_iops_read(struct inode *ip, bool addr_is_userspace, size_t dst,
                        size_t off, size_t n)
{
    printk("devfs_iops_read\n");
    return 0;
}

ssize_t devfs_iops_link(struct inode *dir, struct inode *ip,
                        char name[NAME_MAX])
{
    printk("devfs_iops_link\n");
    return -EOTHER;
}

ssize_t devfs_iops_unlink(struct inode *dir, char name[NAME_MAX],
                          bool delete_files, bool delete_directories)
{
    printk("devfs_iops_unlink\n");
    return 0;
}

// inode operations
struct inode_operations devfs_i_op = {
    iops_create : devfs_iops_create,
    iops_open : devfs_iops_open,
    iops_read_in : devfs_iops_read_in,
    iops_dup : devfs_iops_dup,
    iops_put : devfs_iops_put,
    iops_dir_lookup : devfs_iops_dir_lookup,
    iops_dir_link : devfs_iops_dir_link,
    iops_get_dirent : devfs_iops_get_dirent,
    iops_read : devfs_iops_read,
    iops_link : devfs_iops_link,
    iops_unlink : devfs_iops_unlink
};

ssize_t devfs_fops_write(struct file *f, size_t addr, size_t n)
{
    printk("devfs_fops_write\n");
    return 0;
}

struct file_operations devfs_f_op = {fops_write : devfs_fops_write};

void devfs_init()
{
    devfs_file_system_type.name = DEV_FS_NAME;

    devfs_file_system_type.next = NULL;
    devfs_file_system_type.init_fs_super_block = devfs_init_fs_super_block;
    devfs_file_system_type.kill_sb = devfs_kill_sb;

    // init inodes:
    spin_lock_init(&devfs_itable.lock, "devfs_itable");
    for (size_t i = 0; i < DEVFS_MAX_ACTIVE_INODES; i++)
    {
        devfs_itable.inode[i].dev = MKDEV(DEVFS_MAJOR, 0);
        devfs_itable.inode[i].inum = DEVFS_INVALID_INODE_NUMBER;
        devfs_itable.inode[i].ref = 0;
        devfs_itable.inode[i].i_sb = NULL;
        sleep_lock_init(&devfs_itable.inode[i].lock, "devfs inode");
        devfs_itable.inode[i].valid = true;
        devfs_itable.inode[i].i_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
        devfs_itable.inode[i].nlink = 1;
        devfs_itable.inode[i].size = 0;
        devfs_itable.inode[i].is_mounted_on = NULL;
        devfs_itable.name[i] = NULL;
    }

    register_file_system(&devfs_file_system_type);
}

ssize_t devfs_init_fs_super_block(struct super_block *sb_in, const void *data)
{
    sb_in->s_fs_info = NULL;
    sb_in->s_type = &devfs_file_system_type;
    sb_in->s_op = &devfs_s_op;
    sb_in->i_op = &devfs_i_op;
    sb_in->f_op = &devfs_f_op;
    sb_in->dev = MKDEV(DEVFS_MAJOR, 0);

    // inode 0 is the dev root directory:
    devfs_itable.inode[0].inum = 0;
    devfs_itable.inode[0].i_mode |= S_IFDIR;
    devfs_itable.inode[0].i_sb = sb_in;
    devfs_itable.inode[0].ref = 1;
    devfs_itable.used_inodes = 1;

    size_t found_devices = 0;
    for (size_t i = 0; i < MAX_DEVICES * MAX_MINOR_DEVICES; i++)
    {
        struct Device *dev = g_devices[i];
        if (dev != NULL)
        {
            size_t inode_idx = devfs_itable.used_inodes;

            devfs_itable.inode[inode_idx].inum = found_devices;
            devfs_itable.inode[inode_idx].i_sb = sb_in;
            if (dev->type == CHAR)
            {
                devfs_itable.inode[inode_idx].i_mode |= S_IFCHR;
            }
            else
            {
                devfs_itable.inode[inode_idx].i_mode |= S_IFBLK;
            }
            devfs_itable.inode[inode_idx].dev = dev->device_number;
            devfs_itable.inode[inode_idx].ref = 1;
            devfs_itable.name[inode_idx] = dev->name;
            devfs_itable.used_inodes = inode_idx + 1;

            found_devices++;
        }
    }

    return 0;
}

void devfs_kill_sb(struct super_block *sb_in) { printk("devfs_kill_sb\n"); }
