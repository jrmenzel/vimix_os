/* SPDX-License-Identifier: MIT */

#include <drivers/bdev/virtio_disk.h>
#include <drivers/bdev/virtio_disk_sysfs.h>
#include <fs/sysfs/sysfs_helper.h>
#include <kernel/errno.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>

enum VIRTIO_DISK_ATTRIBUTE_INDEX
{
    VIRTIO_DISK_SYNC = 0,
};

struct sysfs_attribute virtio_disk_attributes[] = {
    [VIRTIO_DISK_SYNC] = {.name = "sync", .mode = 0600}};

syserr_t virtio_disk_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                    char *buf, size_t n)
{
    struct Device *dev = device_from_kobj(kobj);
    struct Block_Device *bdev = block_device_from_device(dev);
    // struct Generic_Disc *gdisk = generic_disk_from_block_device(bdev);
    // struct Virtio_Disk *vdisk = virtio_disk_from_generic_disk(gdisk);

    syserr_t ret = 0;
    switch (attribute_idx)
    {
        case VIRTIO_DISK_SYNC:
            ret = snprintf(buf, n, "%d\n", bdev->ops.flush ? 1 : 0);
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

syserr_t virtio_disk_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                     const char *buf, size_t n)
{
    struct Device *dev = device_from_kobj(kobj);
    struct Block_Device *bdev = block_device_from_device(dev);

    bool ok;
    int32_t value = store_param_to_int(buf, n, &ok);
    if (!ok)
    {
        return -EINVAL;
    }

    syserr_t ret = -EINVAL;
    switch (attribute_idx)
    {
        case VIRTIO_DISK_SYNC:
        {
            if (value < 0 || value > 2)
            {
                break;
            }
            if (value == 1)
            {
                bdev->ops.flush = virtio_block_device_flush;
            }
            else
            {
                bdev->ops.flush = NULL;
            }
            ret = n;
        }
        break;
        default: break;
    }

    return ret;
}

struct sysfs_ops virtio_disk_sysfs_ops = {
    .show = virtio_disk_sysfs_ops_show,
    .store = virtio_disk_sysfs_ops_store,
};

const struct kobj_type virtio_disk_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &virtio_disk_sysfs_ops,
    .attribute = virtio_disk_attributes,
    .n_attributes =
        sizeof(virtio_disk_attributes) / sizeof(virtio_disk_attributes[0])};
