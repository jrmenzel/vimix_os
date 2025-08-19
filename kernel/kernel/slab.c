/* SPDX-License-Identifier: MIT */

#include <kernel/kalloc.h>
#include <kernel/slab.h>
#include <kernel/string.h>

struct kmem_slab *kmem_slab_create(size_t size)
{
    struct kmem_slab *slab = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (!slab) return NULL;
    size = ROUND_TO_SLAB_ALIGNMENT(size);
    if (size > MAX_SLAB_SIZE)
    {
        panic("kmem_cache_init: unsupported slab size");
    }

    list_init(&(slab->slab_list));
    slab->object_size = size;
    slab->free_list = NULL;
    slab->objects_allocated = 0;

    size_t offset_objects = ROUND_TO_SLAB_ALIGNMENT(sizeof(struct kmem_slab));

    size_t next_object = offset_objects;
    while ((next_object + size) < PAGE_SIZE)
    {
        size_t *object = (size_t *)((size_t)slab + next_object);
        *object = (size_t)slab->free_list;
        slab->free_list = (void *)object;

        next_object += size;
    }

    // printk("slab 0x%zx created, object size = %zd\n", (size_t)slab, size);
    return slab;
}

void *kmem_slab_alloc(struct kmem_slab *slab)
{
    if (slab->free_list == NULL) return NULL;

    void *object = slab->free_list;
    slab->free_list = *(void **)object;
    slab->objects_allocated++;

    memset(object, 0, slab->object_size);
    return object;
}

void kmem_slab_free(struct kmem_slab *slab, void *object)
{
    if (PAGE_ROUND_DOWN((size_t)object) != (size_t)slab)
    {
        panic("kmem_slab_free called for object not belonging to this slab");
    }

#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
    memset((char *)object, 2,
           slab->object_size);  // fill with junk, first size_t will be
                                // overwritten next
#endif                          // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE

    *(void **)object = slab->free_list;
    slab->free_list = object;
    slab->objects_allocated--;
}

void kmem_cache_init(struct kmem_cache *new_cache, const char *name,
                     size_t size)
{
    size = ROUND_TO_SLAB_ALIGNMENT(size);
    if (size > MAX_SLAB_SIZE)
    {
        panic("kmem_cache_init: unsupported slab size");
    }

    spin_lock_init(&new_cache->lock, "kmem_cache");

    strncpy(new_cache->name, name, KMEM_CACHE_MAX_NAME_LEN - 1);
    new_cache->name[KMEM_CACHE_MAX_NAME_LEN - 1] = 0;

    list_init(&(new_cache->slab_list));

    new_cache->object_size = size;
}

void *kmem_cache_alloc(struct kmem_cache *cache)
{
    spin_lock(&cache->lock);

    void *allocation = NULL;

    struct list_head *slab;
    list_for_each(slab, &cache->slab_list)
    {
        allocation = kmem_slab_alloc((struct kmem_slab *)slab);
        if (allocation) break;  // found space, break loop
    }

    if (allocation == NULL)
    {
        // nothing free in the cache...
        struct kmem_slab *new_slab = kmem_slab_create(cache->object_size);
        if (new_slab)
        {
            list_add_tail(&(new_slab->slab_list), &(cache->slab_list));
            allocation = kmem_slab_alloc(new_slab);
        }
    }

    spin_unlock(&cache->lock);
    return allocation;
}

void kmem_cache_free(struct kmem_cache *cache, void *object)
{
    spin_lock(&cache->lock);

    struct kmem_slab *slab = kmem_slab_infer_slab(object);
    kmem_slab_free(slab, object);

    if (kmem_slab_is_empty(slab))
    {
        // printk("delete slab\n");
        list_del(&(slab->slab_list));
        kmem_slab_delete(slab);
    }

    spin_unlock(&cache->lock);
}
