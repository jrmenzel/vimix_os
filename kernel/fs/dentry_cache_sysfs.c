/* SPDX-License-Identifier: MIT */

#include <fs/dentry_cache.h>
#include <fs/dentry_cache_sysfs.h>
#include <fs/sysfs/sysfs_data.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>

int32_t atoi(const char *string);

enum DENTRY_CACHE_ATTRIBUTE_INDEX
{
    DC_LRU_SIZE = 0,
    DC_MAX_LRU_SIZE = 1,
    DC_CLEAR_LRU = 2
};

struct sysfs_attribute dentry_cache_attributes[] = {
    [DC_LRU_SIZE] = {.name = "lru_size", .mode = 0444},
    [DC_MAX_LRU_SIZE] = {.name = "max_lru_size", .mode = 0644},
    [DC_CLEAR_LRU] = {.name = "clear_lru", .mode = 0600}};

syserr_t dentry_cache_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                     char *buf, size_t n)
{
    struct dentry_cache *dcache = dentry_cache_from_kobj(kobj);

    syserr_t ret = 0;

    spin_lock(&dcache->list_lock);
    switch (attribute_idx)
    {
        case DC_LRU_SIZE:
            ret = snprintf(buf, n, "%zu\n", dcache->lru_size);
            break;
        case DC_MAX_LRU_SIZE:
            ret = snprintf(buf, n, "%zu\n", dcache->max_lru_size);
            break;
        case DC_CLEAR_LRU: ret = -EINVAL; break;
        default: ret = -ENOENT; break;
    }
    spin_unlock(&dcache->list_lock);

    if (ret == -1)
    {
        // snprintf error
        ret = -EOTHER;
    }

    return ret;
}

syserr_t dentry_cache_sysfs_ops_store(struct kobject *kobj,
                                      size_t attribute_idx, const char *buf,
                                      size_t n)
{
    struct dentry_cache *dcache = dentry_cache_from_kobj(kobj);

    int32_t value = atoi(buf);

    ssize_t ret = 0;
    switch (attribute_idx)
    {
        case DC_LRU_SIZE: ret = -EINVAL; break;
        case DC_MAX_LRU_SIZE:
            ret = dentry_cache_set_max_lru(dcache, value);
            break;
        case DC_CLEAR_LRU: ret = dentry_cache_clear_lru(dcache); break;
        default: ret = -ENOENT; break;
    }

    if (ret == 0)
    {
        // no error, signal all bytes have been written
        return n;
    }
    return ret;
}

struct sysfs_ops dentry_cache_sysfs_ops = {
    .show = dentry_cache_sysfs_ops_show,
    .store = dentry_cache_sysfs_ops_store,
};

const struct kobj_type dentry_cache_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &dentry_cache_sysfs_ops,
    .attribute = dentry_cache_attributes,
    .n_attributes =
        sizeof(dentry_cache_attributes) / sizeof(dentry_cache_attributes[0])};
