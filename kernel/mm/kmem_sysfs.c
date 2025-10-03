/* SPDX-License-Identifier: MIT */

#include <init/main.h>  // bss_start, bss_end
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

struct sysfs_attribute km_attributes[] = {
    [KM_MEM_TOTAL] = {.name = "mem_total", .mode = 0444},
    [KM_MEM_FREE] = {.name = "mem_free", .mode = 0444},
    [KM_PAGES_ALLOC] = {.name = "pages_alloc", .mode = 0444},
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

    ssize_t ret = -1;
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
        default: break;
    }

    return ret;
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
