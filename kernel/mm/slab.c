/* SPDX-License-Identifier: MIT */

#include <kernel/string.h>
#include <lib/minmax.h>
#include <mm/kalloc.h>
#include <mm/kmem_sysfs.h>
#include <mm/slab.h>

struct kmem_slab *kmem_slab_create(size_t size)
{
    struct kmem_slab *slab = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (!slab) return NULL;
    size = ROUND_TO_MIN_SLAB_SIZE(size);
    if (size > MAX_SLAB_SIZE)
    {
        panic("kmem_slab_create: unsupported slab size");
    }

    list_init(&(slab->slab_list));
    slab->object_size = size;
    slab->free_list = NULL;
    slab->objects_allocated = 0;
    slab->owning_cache = NULL;

    // calculate offset of first object
    size_t offset_objects = ROUND_TO_MIN_SLAB_SIZE(sizeof(struct kmem_slab));

    // create free list
    size_t next_object = offset_objects;
    while ((next_object + size) <= PAGE_SIZE)
    {
        size_t *object = (size_t *)((size_t)slab + next_object);
        *object = (size_t)slab->free_list;
        slab->free_list = (void *)object;

        next_object += size;
    }

    // printk("slab 0x%zx | %zd created\n", (size_t)slab, size);
    return slab;
}

void *kmem_slab_alloc(struct kmem_slab *slab, int32_t flags)
{
    if (slab->free_list == NULL) return NULL;

    void *object = slab->free_list;
    slab->free_list = *(void **)object;
    slab->objects_allocated++;

    if (flags & ALLOC_FLAG_ZERO_MEMORY)
    {
        memset(object, 0, slab->object_size);
    }
    return object;
}

void kmem_slab_free(struct kmem_slab *slab, void *object)
{
    DEBUG_EXTRA_PANIC(
        (PAGE_ROUND_DOWN((size_t)object) == (size_t)slab),
        "kmem_slab_free called for object not belonging to this slab");
    DEBUG_EXTRA_PANIC((slab->owning_cache != NULL),
                      "kmem_slab_free slab not owned by a cache");

    // try to cache free() calls with wrong pointers
    DEBUG_EXTRA_PANIC(
        (((size_t)object % PAGE_SIZE -
          ROUND_TO_MIN_SLAB_SIZE(sizeof(struct kmem_slab))) %
         slab->object_size) == 0,
        "kmem_slab_free not a pointer returned by kmem_slab_alloc()");

#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
    memset((char *)object, 2,
           slab->object_size);  // fill with junk, first size_t will be
                                // overwritten next
#endif                          // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE

    *(void **)object = slab->free_list;
    slab->free_list = object;
    slab->objects_allocated--;
}

size_t kmem_slab_get_free_count(struct kmem_slab *slab)
{
    return kmem_slab_get_max_objects(slab) - slab->objects_allocated;
}

size_t kmem_slab_get_object_count(struct kmem_slab *slab)
{
    return slab->objects_allocated;
}

int isprint(int arg) { return (arg >= 32 && arg <= 126); }

void debug_kmem_slab_dump_objects(struct kmem_slab *slab)
{
    size_t max_objects =
        (PAGE_SIZE - sizeof(struct kmem_slab)) / slab->object_size;

    size_t offset_objects = slab->object_size;
    while (offset_objects < sizeof(struct kmem_slab))
    {
        offset_objects += slab->object_size;
    }

    for (size_t i = 0; i < max_objects; i++)
    {
        size_t obj_offset =
            (size_t)slab + offset_objects + i * slab->object_size;
        bool print = false;
        size_t first_word = *(size_t *)obj_offset;
        if (first_word > (size_t)slab &&
            first_word < ((size_t)slab + PAGE_SIZE))
        {
            // pointer to somewhere in this slab, don't print
            continue;
        }
        for (size_t j = 1; j < slab->object_size / sizeof(size_t); j++)
        {
            if (*(size_t *)(obj_offset + j) != 0)
            {
                print = true;
                break;
            }
        }
        if (print)
        {
            printk("obj %zd: ", i);
            for (size_t j = 0; j < slab->object_size; j++)
            {
                printk("%x ", *(uint8_t *)(obj_offset + j));
            }
            for (size_t j = 0; j < slab->object_size; j++)
            {
                char c = *(char *)(obj_offset + j);
                if (isprint(c))
                {
                    printk("%c", c);
                }
                else
                {
                    printk(".");
                }
            }
            printk("\n");
        }
    }
}

size_t kmem_slab_get_max_objects(struct kmem_slab *slab)
{
    return (PAGE_SIZE - sizeof(struct kmem_slab)) / slab->object_size;
}

void kmem_slab_check(struct kmem_slab *slab)
{
    if (slab->objects_allocated == 0)
    {
        printk("kmem_slab_check: slab 0x%zx is empty, owning: 0x%zx\n",
               (size_t)slab, (size_t)slab->owning_cache);
        debug_kmem_slab_dump_objects(slab);
    }
    DEBUG_EXTRA_PANIC((slab->owning_cache != NULL),
                      "kmem_slab_free slab not owned by a cache");
}
