/* SPDX-License-Identifier: MIT */

#include <fs/sysfs/sysfs_helper.h>
#include <kernel/bio.h>
#include <kernel/bio_sysfs.h>
#include <kernel/errno.h>
#include <kernel/kobject.h>
#include <mm/kmem_sysfs.h>

// /sys/kmem/bio

enum BIO_ATTRIBUTE_INDEX
{
    BIO_NUM = 0,
    BIO_FREE,
    BIO_MIN,
    BIO_MAX_FREE
};

struct sysfs_attribute bio_attributes[] = {
    [BIO_NUM] = {.name = "num", .mode = 0444},
    [BIO_FREE] = {.name = "free", .mode = 0444},
    [BIO_MIN] = {.name = "min", .mode = 0644},
    [BIO_MAX_FREE] = {.name = "max_free", .mode = 0644}};

syserr_t bio_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                            char *buf, size_t n)
{
    struct bio_cache *cache = bio_cache_from_kobj(kobj);

    syserr_t ret = 0;

    spin_lock(&cache->lock);
    switch (attribute_idx)
    {
        case BIO_NUM:
            ret = snprintf(buf, n, "%zu\n", cache->num_buffers);
            break;
        case BIO_FREE:
            ret = snprintf(buf, n, "%zu\n", cache->free_buffers);
            break;
        case BIO_MIN:
            ret = snprintf(buf, n, "%zu\n", cache->min_buffers);
            break;
        case BIO_MAX_FREE:
            ret = snprintf(buf, n, "%zu\n", cache->max_free_buffers);
            break;
        default: ret = -ENOENT; break;
    }
    spin_unlock(&cache->lock);

    if (ret == -1)
    {
        // snprintf error
        ret = -EOTHER;
    }

    return ret;
}

syserr_t bio_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                             const char *buf, size_t n)
{
    struct bio_cache *cache = bio_cache_from_kobj(kobj);
    spin_lock(&cache->lock);

    bool ok;
    int32_t value = store_param_to_int(buf, n, &ok);
    if (!ok)
    {
        spin_unlock(&cache->lock);
        return -EINVAL;
    }

    syserr_t ret = 0;
    switch (attribute_idx)
    {
        case BIO_NUM: ret = -EINVAL; break;
        case BIO_FREE: ret = -EINVAL; break;
        case BIO_MIN: ret = bio_cache_set_min_buffers(cache, value); break;
        case BIO_MAX_FREE:
            ret = bio_cache_set_max_free_buffers(cache, value);
            break;

        default: ret = -ENOENT; break;
    }
    spin_unlock(&cache->lock);

    if (ret == 0)
    {
        // no error, signal all bytes have been written
        return n;
    }

    return ret;
}

struct sysfs_ops bio_sysfs_ops = {
    .show = bio_sysfs_ops_show,
    .store = bio_sysfs_ops_store,
};

const struct kobj_type bio_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &bio_sysfs_ops,
    .attribute = bio_attributes,
    .n_attributes = sizeof(bio_attributes) / sizeof(bio_attributes[0])};
