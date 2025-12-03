/* SPDX-License-Identifier: MIT */

#include <fs/vimixfs/vimixfs.h>
#include <fs/vimixfs/vimixfs_sysfs.h>
#include <kernel/errno.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>

enum VIMIXFS_ATTRIBUTE_INDEX
{
    VIMIXFS_BLOCKS = 0,
    VIMIXFS_INODES,
    VIMIXFS_LOG_BLOCKS,
    VIMIXFS_DEV,
    VIMIXFS_MOUNT_FLAGS
};

struct sysfs_attribute vimixfs_attributes[] = {
    [VIMIXFS_BLOCKS] = {.name = "blocks", .mode = 0444},
    [VIMIXFS_INODES] = {.name = "inodes", .mode = 0444},
    [VIMIXFS_LOG_BLOCKS] = {.name = "log_blocks", .mode = 0444},
    [VIMIXFS_DEV] = {.name = "dev", .mode = 0444},
    [VIMIXFS_MOUNT_FLAGS] = {.name = "mount_flags", .mode = 0444}};

syserr_t vimixfs_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                char *buf, size_t n)
{
    struct super_block *sb = super_block_from_kobj(kobj);
    struct vimixfs_sb_private *priv =
        (struct vimixfs_sb_private *)sb->s_fs_info;
    struct vimixfs_superblock *xsb = &(priv->sb);

    syserr_t ret = 0;
    switch (attribute_idx)
    {
        case VIMIXFS_BLOCKS:
            ret = snprintf(buf, n, "%d\n", xsb->nblocks);
            break;
        case VIMIXFS_INODES:
            ret = snprintf(buf, n, "%d\n", xsb->ninode_blocks);
            break;
        case VIMIXFS_LOG_BLOCKS:
            ret = snprintf(buf, n, "%d\n", xsb->nlog);
            break;
        case VIMIXFS_DEV: ret = snprintf(buf, n, "%d\n", sb->dev); break;
        case VIMIXFS_MOUNT_FLAGS:
            ret = snprintf(buf, n, "%ld\n", sb->s_mountflags);
            break;
        default: ret = -ENOENT; break;
    }

    if (ret == -1)
    {
        // snprintf error
        ret = -EOTHER;
    }

    return ret;
}

syserr_t vimixfs_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                 const char *buf, size_t n)
{
    return -EINVAL;
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
