/* SPDX-License-Identifier: MIT */

#include <fs/xv6fs/xv6fs.h>
#include <fs/xv6fs/xv6fs_sysfs.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>

struct sysfs_attribute xv6fs_attributes[] = {
    {.name = "blocks", .mode = 0444},
    {.name = "inodes", .mode = 0444},
    {.name = "log_blocks", .mode = 0444},
    {.name = "dev", .mode = 0444},
    {.name = "mount_flags", .mode = 0444}};

ssize_t xv6fs_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                             char *buf, size_t n)
{
    struct super_block *sb = super_block_from_kobj(kobj);
    struct xv6fs_sb_private *priv = (struct xv6fs_sb_private *)sb->s_fs_info;
    struct xv6fs_superblock *xsb = &(priv->sb);

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

ssize_t xv6fs_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                              const char *buf, size_t n)
{
    return -1;
}

struct sysfs_ops xv6fs_sysfs_ops = {
    .show = xv6fs_sysfs_ops_show,
    .store = xv6fs_sysfs_ops_store,
};

const struct kobj_type xv6fs_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &xv6fs_sysfs_ops,
    .attribute = xv6fs_attributes,
    .n_attributes = sizeof(xv6fs_attributes) / sizeof(xv6fs_attributes[0])};
