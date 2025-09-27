/* SPDX-License-Identifier: MIT */

#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/kmem_sysfs.h>
#include <kernel/kobject.h>
#include <kernel/slab.h>

// /sys/kmem

struct sysfs_attribute km_attributes[] = {
    {.name = "mem_total", .mode = 0444},
    {.name = "mem_free", .mode = 0444},
    {.name = "pages_alloc", .mode = 0444}};

ssize_t km_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx, char *buf,
                          size_t n)
{
    switch (attribute_idx)
    {
        case 0: return snprintf(buf, n, "%zu\n", kalloc_get_total_memory());
        case 1: return snprintf(buf, n, "%zu\n", kalloc_get_free_memory());
        case 2: return snprintf(buf, n, "%zu\n", kalloc_get_allocation_count());
        default: break;
    }

    return -1;
}

ssize_t km_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                           const char *buf, size_t n)
{
    return -1;
}

struct sysfs_ops km_sysfs_ops = {
    .show = km_sysfs_ops_show,
    .store = km_sysfs_ops_store,
};

const struct kobj_type km_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &km_sysfs_ops,
    .attribute = km_attributes,
    .n_attributes = sizeof(km_attributes) / sizeof(km_attributes[0])};

// /sys/kmem/cache_<size>

struct sysfs_attribute kmem_cache_attributes[] = {
    {.name = "slab_count", .mode = 0444},
    {.name = "obj_size", .mode = 0444},
    {.name = "obj_count", .mode = 0444},
    {.name = "obj_max", .mode = 0444}};

ssize_t kmem_cache_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                  char *buf, size_t n)
{
    struct kmem_cache *cache = kmem_cache_from_kobj(kobj);

    switch (attribute_idx)
    {
        case 0:
            return snprintf(buf, n, "%zu\n", kmem_cache_get_slab_count(cache));
        case 1:
            return snprintf(buf, n, "%zu\n", kmem_cache_get_object_size(cache));
        case 2:
            return snprintf(buf, n, "%zu\n",
                            kmem_cache_get_object_count(cache));
        case 3:
            return snprintf(buf, n, "%zu\n", kmem_cache_get_max_objects(cache));
        default: break;
    }

    return -1;
}

ssize_t kmem_cache_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                   const char *buf, size_t n)
{
    return -1;
}

struct sysfs_ops kmem_cache_sysfs_ops = {
    .show = kmem_cache_sysfs_ops_show,
    .store = kmem_cache_sysfs_ops_store,
};

const struct kobj_type kmem_cache_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &kmem_cache_sysfs_ops,
    .attribute = kmem_cache_attributes,
    .n_attributes =
        sizeof(kmem_cache_attributes) / sizeof(kmem_cache_attributes[0])};
