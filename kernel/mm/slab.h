/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>
#include <mm/kalloc.h>

// Pick 16 bytes as the smallest cache
#define SLAB_ALIGNMENT_ORDER 4

// Slab objects alignment in bytes. Also the minimal size of an object in the
// slab. Ideally hardware cache aligned.
#define SLAB_ALIGNMENT (1 << SLAB_ALIGNMENT_ORDER)

_Static_assert((sizeof(size_t) <= SLAB_ALIGNMENT),
               "Slabs manage free space with a linked list in free objects, so "
               "an object can not be smaller than a size_t");

// 1<<2 == 4 -> 1/4
#define MAX_SLAB_SIZE_DIVIDER_SHIFT 2

// maximal size of objects managed by the slab allocator
// a full slab is stored in one page, data and metadata.
// Minus the slab struct only three 1024 byte objects fit into
// one 4KB page, this is still useful. However only one object of half the
// PAGE_SIZE would fit, so for everything larger 1/4 of a PAGE_SIZE kmalloc
// will use a full page from alloc_page() instead.
#define MAX_SLAB_SIZE (PAGE_SIZE / (1 << MAX_SLAB_SIZE_DIVIDER_SHIFT))

// round up an allocation size to the next multiple of the SLAB_ALIGNMENT
#define ROUND_TO_SLAB_ALIGNMENT(size) \
    (((size + SLAB_ALIGNMENT - 1) / SLAB_ALIGNMENT) * SLAB_ALIGNMENT)

/// @brief  A slab allocator managing one page of memory, used by kmem_cache.
/// Access must be synced externally. Don't use directly, use a kmem_cache
/// object.
struct kmem_slab
{
    // double linked list to all other slabs managing the same allocation
    // type/size
    struct list_head slab_list;
    // free objects in this slab
    void *free_list;
    // size of one object including padding to the SLAB_ALIGNMENT
    size_t object_size;
    // number of allocated objects, used to detect when a slab is empty
    size_t objects_allocated;

    // if the slab is managed by a cache, this points to it
    // can be NULL if the slab is used standalone
    struct kmem_cache *owning_cache;
};

#define kmem_slab_from_list(ptr) container_of(ptr, struct kmem_slab, slab_list)

/// @brief Construct a new slab object.
struct kmem_slab *kmem_slab_create(size_t size);

/// @brief True if no objects are managed by this slab.
/// @param slab The slab to test.
/// @return True if empty
static inline bool kmem_slab_is_empty(struct kmem_slab *slab)
{
    return (slab->objects_allocated == 0);
}

/// @brief Delete a slab
/// @param The slab created by kmem_slab_create()
static inline void kmem_slab_delete(struct kmem_slab *slab)
{
    DEBUG_EXTRA_ASSERT(kmem_slab_is_empty(slab),
                       "deleting non empty slab container!");
    free_page(slab);
}

/// @brief Allocate a new object from this slab.
/// @param slab Slab to get an object from, size is implicit by the chosen slab.
/// @return NULL if the slab is full already.
void *kmem_slab_alloc(struct kmem_slab *slab, int32_t flags);

/// @brief If we know that an object was allocated by some slab, we can infer
/// the slab as its struct is stored at the beginning of the same page.
/// Only works as long as slab allocators only manage one page each.
static inline struct kmem_slab *kmem_slab_infer_slab(void *object)
{
    return (struct kmem_slab *)PAGE_ROUND_DOWN((size_t)object);
}

/// @brief Free an object. Use kmem_slab_infer_slab() if the slab used was not
/// stored explicitly.
/// @param slab Slab used to originally allocate object from.
/// @param object The object.
void kmem_slab_free(struct kmem_slab *slab, void *object);

/// @brief True if the slab is completely full.
/// @param slab The slab.
/// @return True if full.
static inline bool kmem_slab_is_full(struct kmem_slab *slab)
{
    return (slab->free_list == NULL);
}

/// @brief Get number of free/available objects in this slab.
/// @param slab The slab to query.
/// @return Objects available.
size_t kmem_slab_get_free_count(struct kmem_slab *slab);

/// @brief Get number of allocated objects in this slab.
/// @param slab The slab to query.
/// @return Objects allocated.
size_t kmem_slab_get_object_count(struct kmem_slab *slab);

/// @brief Get the maximum number of objects this slab can manage.
/// @param slab The slab to query.
/// @return Max space in this slab.
size_t kmem_slab_get_max_objects(struct kmem_slab *slab);

void kmem_slab_check(struct kmem_slab *slab);
