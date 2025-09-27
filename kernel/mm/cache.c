/* SPDX-License-Identifier: MIT */

#include <kernel/string.h>
#include <mm/cache.h>
#include <mm/kalloc.h>
#include <mm/kmem_sysfs.h>

void kmem_cache_check(struct kmem_cache *cache)
{
    spin_lock(&cache->lock);
    struct list_head *pos;
    list_for_each(pos, &cache->slab_list)
    {
        struct kmem_slab *slab = kmem_slab_from_list(pos);
        kmem_slab_check(slab);
    }

    spin_unlock(&cache->lock);
}

void kmem_cache_init(struct kmem_cache *new_cache, size_t size)
{
    size = ROUND_TO_SLAB_ALIGNMENT(size);
    if (size > MAX_SLAB_SIZE)
    {
        panic("kmem_cache_init: unsupported slab size");
    }

    spin_lock_init(&new_cache->lock, "kmem_cache");
    list_init(&(new_cache->slab_list));
    new_cache->object_size = size;
    kobject_init(&new_cache->kobj, &kmem_cache_kobj_ktype);
}

void *kmem_cache_alloc(struct kmem_cache *cache, int32_t flags)
{
    spin_lock(&cache->lock);

    void *allocation = NULL;

    struct list_head *slab;
    list_for_each(slab, &cache->slab_list)
    {
        allocation = kmem_slab_alloc((struct kmem_slab *)slab, flags);
        if (allocation) break;  // found space, break loop
    }

    if (allocation == NULL)
    {
        // nothing free in the cache...
        struct kmem_slab *new_slab = kmem_slab_create(cache->object_size);
        if (new_slab)
        {
            list_add_tail(&(new_slab->slab_list), &(cache->slab_list));
            new_slab->owning_cache = cache;
            allocation = kmem_slab_alloc(new_slab, flags);
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
        list_del(&(slab->slab_list));
        kmem_slab_delete(slab);
    }

    spin_unlock(&cache->lock);
}

static inline size_t kmem_cache_get_slab_count_locked(struct kmem_cache *cache)
{
    size_t count = 0;
    struct list_head *slab;
    list_for_each(slab, &cache->slab_list) { count++; }
    return count;
}

size_t kmem_cache_get_slab_count(struct kmem_cache *cache)
{
    spin_lock(&cache->lock);
    size_t count = kmem_cache_get_slab_count_locked(cache);
    spin_unlock(&cache->lock);
    return count;
}

size_t kmem_cache_get_max_objects(struct kmem_cache *cache)
{
    spin_lock(&cache->lock);
    struct kmem_slab *slab = kmem_slab_from_list(cache->slab_list.next);
    size_t count = kmem_slab_get_max_objects(slab) *
                   kmem_cache_get_slab_count_locked(cache);
    spin_unlock(&cache->lock);
    return count;
}

size_t kmem_cache_get_object_size(struct kmem_cache *cache)
{
    // no locking, object_size is constant after init
    return cache->object_size;
}

size_t kmem_cache_get_object_count(struct kmem_cache *cache)
{
    size_t count = 0;
    spin_lock(&cache->lock);
    struct list_head *pos;
    list_for_each(pos, &cache->slab_list)
    {
        struct kmem_slab *slab = kmem_slab_from_list(pos);
        count += kmem_slab_get_object_count(slab);
    }

    spin_unlock(&cache->lock);
    return count;
}
