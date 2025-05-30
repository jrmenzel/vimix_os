/* SPDX-License-Identifier: MIT */

#include <fs/devfs/devfs.h>
#include <fs/vfs.h>
#include <fs/xv6fs/xv6fs.h>
#include <kernel/string.h>

struct file_system_type *g_file_systems;
extern struct sleeplock g_mount_lock;

void init_virtual_file_system()
{
    g_file_systems = NULL;
    sleep_lock_init(&g_mount_lock, "mount");

    // init all file system implementations
    devfs_init();
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
