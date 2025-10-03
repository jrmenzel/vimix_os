/* SPDX-License-Identifier: MIT */

#include <fs/vimixfs/vimixfs.h>
#include <fs/vimixfs/vimixfs_sysfs.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>

struct sysfs_attribute vimixfs_attributes[] = {
    {.name = "blocks", .mode = 0444},
    {.name = "inodes", .mode = 0444},
    {.name = "log_blocks", .mode = 0444},
    {.name = "dev", .mode = 0444},
    {.name = "mount_flags", .mode = 0444}};

ssize_t vimixfs_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                               char *buf, size_t n)
{
    struct super_block *sb = super_block_from_kobj(kobj);
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb->s_fs_info;
    struct vimixfs_superblock *xsb = &(priv->sb);

    ssize_t ret = -1;
    switch (attribute_idx)
    {
        case 0: ret = snprintf(buf, n, "%d\n", xsb->nblocks); break;
        case 1: ret = snprintf(buf, n, "%d\n", xsb->ninodes); break;
        case 2: ret = snprintf(buf, n, "%d\n", xsb->nlog); break;
        case 3: ret = snprintf(buf, n, "%d\n", sb->dev); break;
        case 4: ret = snprintf(buf, n, "%ld\n", sb->s_mountflags); break;
        default: break;
    }

    return ret;
}

ssize_t vimixfs_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                const char *buf, size_t n)
{
    return -1;
}

struct sysfs_ops vimixfs_sysfs_ops = {
    .show = vimixfs_sysfs_ops_show,
    .store = vimixfs_sysfs_ops_store,
};

const struct kobj_type vimixfs_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &vimixfs_sysfs_ops,
    .attribute = vimixfs_attributes,
    .n_attributes = sizeof(vimixfs_attributes) / sizeof(vimixfs_attributes[0])};
