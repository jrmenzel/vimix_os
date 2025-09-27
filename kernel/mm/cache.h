/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>
#include <kernel/spinlock.h>
#include <mm/slab.h>

#define KMEM_CACHE_MAX_NAME_LEN 16

/// @brief A cache of allocations of a certain type/size.
/// kmalloc() is using kmem_cache objects for various allocation sizes.
/// Grows and shrinks internal list of slabs.
struct kmem_cache
{
    // kobject for sysfs
    struct kobject kobj;

    // lock protecting this cache
    struct spinlock lock;

    // double linked list of slabs providing the cache memory
    struct list_head slab_list;

    // size of one object including padding to the SLAB_ALIGNMENT
    size_t object_size;

    // name for debugging
    char name[KMEM_CACHE_MAX_NAME_LEN];
};

#define kmem_cache_from_kobj(ptr) container_of(ptr, struct kmem_cache, kobj)

/// @brief Init a cache for objects of a given size.
/// @param size Size per object in bytes.
void kmem_cache_init(struct kmem_cache *new_cache, size_t size);

/// @brief Allocate an object from this cache.
/// @param cache The cache to use.
/// @return An object or NULL if out of memory.
void *kmem_cache_alloc(struct kmem_cache *cache);

/// @brief Free an object.
/// @param cache The cache to use.
/// @param object Pointer to object to free.
void kmem_cache_free(struct kmem_cache *cache, void *object);

/// @brief Get the number of slabs in this cache.
/// @param cache The cache to query.
/// @return Count of slabs, each slab is one page.
size_t kmem_cache_get_slab_count(struct kmem_cache *cache);

/// @brief How many objects can this cache manage in total currently.
/// @param cache The cahce to query.
/// @return Count of objects.
size_t kmem_cache_get_max_objects(struct kmem_cache *cache);

/// @brief Get the size of objects in this cache.
/// @param cache The cache to query.
/// @return Size in bytes.
size_t kmem_cache_get_object_size(struct kmem_cache *cache);

/// @brief Get the number of allocated objects in this cache.
/// @param cache Cache to query.
/// @return Count of allocated objects in all slabs of this cache.
size_t kmem_cache_get_object_count(struct kmem_cache *cache);

void kmem_cache_check(struct kmem_cache *cache);
