/* SPDX-License-Identifier: MIT */

#include <init/main.h>  // bss_start, bss_end
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <mm/cache.h>
#include <mm/kalloc.h>
#include <mm/kernel_memory.h>
#include <mm/kmem_sysfs.h>

// /sys/kmem

enum KM_ATTRIBUTE_INDEX
{
    KM_MEM_TOTAL = 0,
    KM_MEM_FREE,
    KM_PAGES_ALLOC,
    KM_MEM_ALLOC,
    KM_KERNEL_START,
    KM_KERNEL_END,
    KM_BSS_START,
    KM_BSS_END,
    KM_RAM_START,
    KM_RAM_END,
    KM_INITRD_START,
    KM_INITRD_END,
    KM_DTB_START,
    KM_DTB_END
};

struct sysfs_attribute kmem_attributes[] = {
    [KM_MEM_TOTAL] = {.name = "mem_total", .mode = 0444},
    [KM_MEM_FREE] = {.name = "mem_free", .mode = 0444},
    [KM_PAGES_ALLOC] = {.name = "pages_alloc", .mode = 0444},
    [KM_MEM_ALLOC] = {.name = "mem_alloc", .mode = 0444},
    [KM_KERNEL_START] = {.name = "kernel_start", .mode = 0444},
    [KM_KERNEL_END] = {.name = "kernel_end", .mode = 0444},
    [KM_BSS_START] = {.name = "bss_start", .mode = 0444},
    [KM_BSS_END] = {.name = "bss_end", .mode = 0444},
    [KM_RAM_START] = {.name = "ram_start", .mode = 0444},
    [KM_RAM_END] = {.name = "ram_end", .mode = 0444},
    [KM_INITRD_START] = {.name = "initrd_start", .mode = 0444},
    [KM_INITRD_END] = {.name = "initrd_end", .mode = 0444},
    [KM_DTB_START] = {.name = "dtb_start", .mode = 0444},
    [KM_DTB_END] = {.name = "dtb_end", .mode = 0444}};

ssize_t km_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx, char *buf,
                          size_t n)
{
    struct kernel_memory *kmem = kernel_memory_from_kobj(kobj);
    struct Minimal_Memory_Map *map = &kmem->memory_map;

    ssize_t ret = 0;
    switch (attribute_idx)
    {
        case KM_MEM_TOTAL:
            ret = snprintf(buf, n, "%zu\n", kalloc_get_total_memory());
            break;
        case KM_MEM_FREE:
            ret = snprintf(buf, n, "%zu\n", kalloc_get_free_memory());
            break;
        case KM_PAGES_ALLOC:
            ret = snprintf(buf, n, "%zu\n", kalloc_get_allocation_count());
            break;
        case KM_MEM_ALLOC:
            ret = snprintf(buf, n, "%zu\n", kalloc_get_memory_allocated());
            break;
        case KM_KERNEL_START:
            ret = snprintf(buf, n, "%zu\n", map->kernel_start);
            break;
        case KM_KERNEL_END:
            ret = snprintf(buf, n, "%zu\n", map->kernel_end);
            break;
        case KM_BSS_START:
            ret = snprintf(buf, n, "%zu\n", (size_t)bss_start);
            break;
        case KM_BSS_END:
            ret = snprintf(buf, n, "%zu\n", (size_t)bss_end);
            break;
        case KM_RAM_START:
            ret = snprintf(buf, n, "%zu\n", map->ram_start);
            break;
        case KM_RAM_END: ret = snprintf(buf, n, "%zu\n", map->ram_end); break;
        case KM_INITRD_START:
            ret = snprintf(buf, n, "%zu\n", map->initrd_begin);
            break;
        case KM_INITRD_END:
            ret = snprintf(buf, n, "%zu\n", map->initrd_end);
            break;
        case KM_DTB_START:
            ret = snprintf(buf, n, "%zu\n", map->dtb_file_start);
            break;
        case KM_DTB_END:
            ret = snprintf(buf, n, "%zu\n", map->dtb_file_end);
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

ssize_t km_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                           const char *buf, size_t n)
{
    return -1;
}

struct sysfs_ops kmem_sysfs_ops = {
    .show = km_sysfs_ops_show,
    .store = km_sysfs_ops_store,
};

const struct kobj_type kmem_kobj_ktype = {
    .release = NULL,
    .sysfs_ops = &kmem_sysfs_ops,
    .attribute = kmem_attributes,
    .n_attributes = sizeof(kmem_attributes) / sizeof(kmem_attributes[0])};

// /sys/kmem/cache_<size>

enum KMEM_CACHE_ATTRIBUTE_INDEX
{
    KM_SLAB_COUNT = 0,
    KM_OBJ_SIZE,
    KM_OBJ_COUNT,
    KM_OBJ_MAX
};

struct sysfs_attribute kmem_cache_attributes[] = {
    [KM_SLAB_COUNT] = {.name = "slab_count", .mode = 0444},
    [KM_OBJ_SIZE] = {.name = "obj_size", .mode = 0444},
    [KM_OBJ_COUNT] = {.name = "obj_count", .mode = 0444},
    [KM_OBJ_MAX] = {.name = "obj_max", .mode = 0444}};

syserr_t kmem_cache_sysfs_ops_show(struct kobject *kobj, size_t attribute_idx,
                                   char *buf, size_t n)
{
    struct kmem_cache *cache = kmem_cache_from_kobj(kobj);

    syserr_t ret = 0;
    switch (attribute_idx)
    {
        case KM_SLAB_COUNT:
            ret = snprintf(buf, n, "%zu\n", kmem_cache_get_slab_count(cache));
            break;
        case KM_OBJ_SIZE:
            ret = snprintf(buf, n, "%zu\n", kmem_cache_get_object_size(cache));
            break;
        case KM_OBJ_COUNT:
            ret = snprintf(buf, n, "%zu\n", kmem_cache_get_object_count(cache));
            break;
        case KM_OBJ_MAX:
            ret = snprintf(buf, n, "%zu\n", kmem_cache_get_max_objects(cache));
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

syserr_t kmem_cache_sysfs_ops_store(struct kobject *kobj, size_t attribute_idx,
                                    const char *buf, size_t n)
{
    return -EINVAL;
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
