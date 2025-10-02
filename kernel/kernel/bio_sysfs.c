/* SPDX-License-Identifier: MIT */

#include <kernel/bio.h>
#include <kernel/bio_sysfs.h>
#include <kernel/kobject.h>
#include <mm/kmem_sysfs.h>

struct sysfs_attribute bio_attributes[] = {{.name = "num", .mode = 0444},
                                           {.name = "free", .mode = 0444},
                                           {.name = "min", .mode = 0444},
                                           {.name = "max_free", .mode = 0444}};

ssize_t bio_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                           char *buf, size_t n)
{
    struct bio_cache *cache = bio_cache_from_kobj(kobj);
    spin_lock(&cache->lock);

    ssize_t ret = -1;
    switch (attribute_idx)
    {
        case 0: ret = snprintf(buf, n, "%zu\n", cache->num_buffers); break;
        case 1: ret = snprintf(buf, n, "%zu\n", cache->free_buffers); break;
        case 2: ret = snprintf(buf, n, "%zu\n", cache->min_buffers); break;
        case 3: ret = snprintf(buf, n, "%zu\n", cache->max_free_buffers); break;
        default: break;
    }

    spin_unlock(&cache->lock);
    return ret;
}

int32_t atoi(const char *string);

ssize_t bio_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                            const char *buf, size_t n)
{
    struct bio_cache *cache = bio_cache_from_kobj(kobj);
    spin_lock(&cache->lock);

    int32_t value = atoi(buf);

    ssize_t ret = -1;
    switch (attribute_idx)
    {
        case 2: ret = bio_cache_set_min_buffers(cache, value); break;
        case 3: ret = bio_cache_set_max_free_buffers(cache, value); break;
        default: break;
    }

    spin_unlock(&cache->lock);
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
