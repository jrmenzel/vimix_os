/* SPDX-License-Identifier: MIT */

#include <fs/devfs/devfs.h>
#include <fs/sysfs/sysfs.h>
#include <fs/vfs.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/errno.h>
#include <kernel/string.h>

struct file_system_type *g_file_systems;
extern struct sleeplock g_mount_lock;

void init_virtual_file_system()
{
    g_file_systems = NULL;
    sleep_lock_init(&g_mount_lock, "mount");

    // init all file system implementations
    devfs_init();
    sysfs_init();
    xv6fs_init();
}

// reused code from Linux kernel ;-)
struct file_system_type **find_filesystem(const char *name, unsigned len)
{
    struct file_system_type **p;
    for (p = &g_file_systems; *p; p = &(*p)->next)
        if (strncmp((*p)->name, name, len) == 0 && !(*p)->name[len]) break;
    return p;
}

void register_file_system(struct file_system_type *fs)
{
    struct file_system_type **p;

    if (fs->next)
    {
        panic("register_file_system: fs->next is not NULL");
    }

    // there shouldn't be an entry for this FS yet, so we expect
    // the next pointer of the last filesystem (pointing to NULL)
    p = find_filesystem(fs->name, strlen(fs->name));

    if (*p != NULL)
    {
        panic("register_file_system: fs registered multiple times");
    }

    // set next pointer of last file system to this new one:
    *p = fs;
}

struct inode *sops_alloc_inode_default_ro(struct super_block *sb, mode_t mode)
{
    return NULL;
}

int sops_write_inode_default_ro(struct inode *ip) { return 0; }

struct inode *iops_create_default_ro(struct inode *iparent, char name[NAME_MAX],
                                     mode_t mode, int32_t flags, dev_t device)
{
    return NULL;
}

struct inode *iops_dup_default(struct inode *ip)
{
    inode_get(ip);
    return ip;
}

void iops_put_default(struct inode *ip)
{
    DEBUG_EXTRA_ASSERT(kref_read(&ip->ref) > 0,
                       "Can't put an inode that is not held by anyone");

    if (kref_put(&ip->ref) == true)
    {
        // last reference dropped
        inode_del(ip);
    }
}

int iops_dir_link_default_ro(struct inode *dir, char *name, ino_t inum)
{
    return 0;
}

ssize_t iops_link_default_ro(struct inode *dir, struct inode *ip,
                             char name[NAME_MAX])
{
    return -EOTHER;
}

ssize_t iops_unlink_default_ro(struct inode *dir, char name[NAME_MAX],
                               bool delete_files, bool delete_directories)
{
    return 0;
}