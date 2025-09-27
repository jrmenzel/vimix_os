/* SPDX-License-Identifier: MIT */

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include <init/main.h>
#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <lib/minmax.h>
#include <mm/cache.h>
#include <mm/kalloc.h>
#include <mm/kmem_sysfs.h>

// The kernel saves free lists for 2^0 to 2^PAGE_ALLOC_MAX_ORDER pages
// it uses a buddy allocator strategy to merge free buddy blocks of
// order X to one of order X+1 and splits higher order blocks if no free
// blocks of the requested order are available.
#define PAGE_ALLOC_MAX_ORDER 9

// Number of Slab Allocator caches to provide allocations from SLAB_ALIGNMENT
// to PAGE_SIZE/4
#define OBJECT_CACHES \
    ((PAGE_SHIFT - SLAB_ALIGNMENT_ORDER) - (MAX_SLAB_SIZE_DIVIDER_SHIFT - 1))

/// a linked list per order of all free memory blocks for the kernel to allocate
struct
{
    struct kobject kobj;
    struct spinlock lock;
    char *end_of_physical_memory;

    struct list_head list_of_free_memory[PAGE_ALLOC_MAX_ORDER + 1];

    struct kmem_cache object_cache[OBJECT_CACHES];

    atomic_size_t pages_allocated;
    struct Minimal_Memory_Map memory_map;

#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
    // detect kmalloc usage before init
    bool kmalloc_initialized;
#endif  // CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
} g_kernel_memory = {0};

// helper which assumes g_kernel_memory.lock is held
// most of the time better call alloc_pages()
void *__alloc_pages(int32_t flags, size_t order)
{
    if (order > PAGE_ALLOC_MAX_ORDER) return NULL;

    void *pages = NULL;
    if (!list_empty(&g_kernel_memory.list_of_free_memory[order]))
    {
        // memory in the right size available
        struct list_head *memory_block =
            g_kernel_memory.list_of_free_memory[order].next;
        list_del(memory_block);
        pages = (void *)memory_block;
    }
    else
    {
        // split a larger allocation
        uint8_t *double_alloc = (uint8_t *)__alloc_pages(flags, order + 1);
        if (double_alloc)
        {
            // return left half, add right half to empty list
            pages = (void *)double_alloc;
            double_alloc += (1 << order) * PAGE_SIZE;
            list_add((struct list_head *)double_alloc,
                     &g_kernel_memory.list_of_free_memory[order]);
        }
    }
    return pages;
}

void zero_pages(void *addr, size_t page_count)
{
    // Could be done with
    //   memset(addr, 0, PAGE_SIZE * page_count);
    // But knowing the page_count is a multiple of PAGE_SIZE
    // and addr is page aligned, the loop can be optimized.

    size_t *addr_s = (size_t *)addr;
    ssize_t to_clear = page_count * PAGE_SIZE;
    while (to_clear > 0)
    {
        addr_s[0] = 0;
        addr_s[1] = 0;
        addr_s[2] = 0;
        addr_s[3] = 0;
        to_clear -= 4 * sizeof(size_t);
        addr_s += 4;
    }
}

void *alloc_pages(int32_t flags, size_t order)
{
    spin_lock(&g_kernel_memory.lock);

    void *pages = __alloc_pages(flags, order);

    if (pages)
    {
        if (flags & ALLOC_FLAG_ZERO_MEMORY)
        {
            zero_pages(pages, (1 << order));
        }
        else
        {
#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
            memset((char *)pages, 5,
                   PAGE_SIZE * (1 << order));  // fill with junk
#endif  // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
        }
        atomic_fetch_add(&g_kernel_memory.pages_allocated, (1 << order));
    }

    spin_unlock(&g_kernel_memory.lock);

    return pages;
}

// helper for merging blocks of memory
void *get_specific_page(size_t pa, size_t order)
{
    if (order > PAGE_ALLOC_MAX_ORDER) return NULL;

    struct list_head *block;
    list_for_each(block, &g_kernel_memory.list_of_free_memory[order])
    {
        if (pa == (size_t)block)
        {
            // found it
            list_del(block);
            return (void *)block;
        }
    }

    return NULL;
}

// helper which assumes g_kernel_memory.lock is held
// most of the time better call free_pages()
void __free_pages(void *pa, size_t order)
{
    void *buddy = NULL;
    if (order < PAGE_ALLOC_MAX_ORDER)
    {
        // merge buddies if possible
        size_t bit_pos = PAGE_SHIFT + order;
        size_t mask = (1 << bit_pos);
        size_t buddy_address = (size_t)pa ^ mask;  // flip lowest address bit
        buddy = get_specific_page(buddy_address, order);
    }

    if (buddy)
    {
        size_t left_buddy = min((size_t)pa, (size_t)buddy);
        __free_pages((void *)left_buddy, order + 1);
    }
    else
    {
#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
        // Fill with junk to catch dangling refs.
        memset(pa, 1, PAGE_SIZE * (1 << order));
#endif  // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
        list_add((struct list_head *)pa,
                 &g_kernel_memory.list_of_free_memory[order]);
    }
}

void free_pages(void *pa, size_t order)
{
    if (order > PAGE_ALLOC_MAX_ORDER)
    {
        panic("free_pages: invalid order");
    }

    spin_lock(&g_kernel_memory.lock);

    __free_pages(pa, order);

    atomic_fetch_add(&g_kernel_memory.pages_allocated, -(1 << order));

    spin_unlock(&g_kernel_memory.lock);
}

void kalloc_init_memory_region(size_t mem_start, size_t mem_end)
{
    size_t addr = mem_start;
    while (addr < mem_end)
    {
        size_t tmp = addr;
        tmp >>= PAGE_SHIFT;
        size_t order = 0;
        while (((tmp & 0x01) == 0) && (order < PAGE_ALLOC_MAX_ORDER))
        {
            tmp >>= 1;
            order++;
        }

        free_pages((void *)addr, order);
        addr += (PAGE_SIZE * (1 << order));
    }
}

void kalloc_init(struct Minimal_Memory_Map *memory_map)
{
    kobject_init(&g_kernel_memory.kobj, &km_kobj_ktype);
    kobject_add(&g_kernel_memory.kobj, &g_kobjects_root, "kmem");

    spin_lock_init(&g_kernel_memory.lock, "kmem");
    g_kernel_memory.memory_map = *memory_map;
    g_kernel_memory.end_of_physical_memory = (char *)memory_map->ram_end;

    for (size_t i = 0; i <= PAGE_ALLOC_MAX_ORDER; ++i)
    {
        list_init(&g_kernel_memory.list_of_free_memory[i]);
    }

    // The available memory after kernel_end can have up to two holes:
    // the dtb file and a initrd ramdisk, both are optional.
    // The dtb could also be outside of the RAM area (e.g. in flash)
    // or inside of the kernel (if it's compiled in).

    size_t region_start = memory_map->kernel_end;
    region_start = PAGE_ROUND_UP(region_start);
    while (true)
    {
        size_t region_end = memory_map->ram_end;
        size_t next_region_start = 0;

        if (memory_map->dtb_file_start != 0)
        {
            if (region_start < memory_map->dtb_file_start)
            {
                region_end = min(region_end, memory_map->dtb_file_start);
                next_region_start = PAGE_ROUND_UP(memory_map->dtb_file_end);
            }
        }
        if (memory_map->initrd_begin != 0)
        {
            if (region_start < memory_map->initrd_begin)
            {
                region_end = min(region_end, memory_map->initrd_begin);
                next_region_start = PAGE_ROUND_UP(memory_map->initrd_end);
            }
        }

        // printk("kalloc memory range: 0x%zx - 0x%zx\n", region_start,
        //        region_end);
        kalloc_init_memory_region(region_start, region_end);
        if (region_end == memory_map->ram_end) break;

        region_start = next_region_start;
    }

    // reset *after* kalloc_init_memory_region() calls (as kfree() decrements
    // the counter)
    atomic_init(&g_kernel_memory.pages_allocated, 0);

    // init object caches for kmalloc()
    for (size_t i = 0; i < OBJECT_CACHES; ++i)
    {
        size_t size = (1 << i) * SLAB_ALIGNMENT;

        kmem_cache_init(&g_kernel_memory.object_cache[i], size);
    }

    // with all caches created, mark kmalloc as initialized
    // now kobject_add() can be called savely, as it might call kmalloc()
#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
    g_kernel_memory.kmalloc_initialized = true;
#endif  // CONFIG_DEBUG_EXTRA_RUNTIME_TESTS

    for (size_t i = 0; i < OBJECT_CACHES; ++i)
    {
        size_t size = (1 << i) * SLAB_ALIGNMENT;
        if (!kobject_add(&g_kernel_memory.object_cache[i].kobj,
                         &g_kernel_memory.kobj, "kmalloc_%zd", size))
        {
            panic("kmem_cache_init: failed to add kobject");
        }
    }
}

void kfree(void *pa)
{
    DEBUG_EXTRA_PANIC(g_kernel_memory.kmalloc_initialized,
                      "kfree called before kalloc_init()");
    if ((char *)pa < end_of_kernel  // or pa is before or inside the kernel
                                    // binary in memory
        || (char *)pa >=
               g_kernel_memory.end_of_physical_memory)  // or pa is outside of
                                                        // the physical memory
    {
        panic("kfree: out of range or unaligned address");
    }

    if (((size_t)pa % PAGE_SIZE) == 0)
    {
        // page aligned
        free_pages(pa, 0);
        return;
    }

    // must be from an object slab / cache
    struct kmem_slab *slab = kmem_slab_infer_slab(pa);
    if (slab->owning_cache == NULL)
    {
        // free the object, but NOT the slab (if empty)
        kmem_slab_free(slab, pa);
    }
    else
    {
        // free the object AND the slab (if empty)
        kmem_cache_free(slab->owning_cache, pa);
    }
}

size_t next_power_of_two(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#ifdef __ARCH_64BIT
    v |= v >> 32;
#endif
    v++;

    return v;
}

void *kmalloc(size_t size)
{
    DEBUG_EXTRA_PANIC(g_kernel_memory.kmalloc_initialized,
                      "kfree called before kalloc_init()");
    if (size > PAGE_SIZE)
    {
        panic("too much memory to allocate for kmalloc()");
    }

    size = next_power_of_two(size);
    size_t order = 0;
    while (size >>= 1)
    {
        order++;
    }
    size_t cache_index = 0;
    if (order > SLAB_ALIGNMENT_ORDER)
    {
        cache_index = order - SLAB_ALIGNMENT_ORDER;
    }
    if (cache_index >= OBJECT_CACHES)
    {
        // no cache for this size -> return a full page
        // printk("alloc full page\n");
        return alloc_pages(ALLOC_FLAG_NONE, 0);
    }
    return kmem_cache_alloc(&(g_kernel_memory.object_cache[cache_index]));
}

size_t kalloc_get_allocation_count()
{
    return atomic_load(&g_kernel_memory.pages_allocated);
}

size_t kalloc_get_total_memory()
{
    return (g_kernel_memory.memory_map.ram_end -
            g_kernel_memory.memory_map.ram_start);
}

size_t kalloc_get_free_memory()
{
    spin_lock(&g_kernel_memory.lock);
    size_t pages = 0;

    for (size_t i = 0; i <= PAGE_ALLOC_MAX_ORDER; ++i)
    {
        struct list_head *block;
        list_for_each(block, &g_kernel_memory.list_of_free_memory[i])
        {
            pages += (1 << i);
        }
    }

    spin_unlock(&g_kernel_memory.lock);

    return pages * PAGE_SIZE;
}

void kalloc_dump_free_memory()
{
    printk("\n");
    for (size_t i = 0; i <= PAGE_ALLOC_MAX_ORDER; ++i)
    {
        size_t blocks = 0;
        struct list_head *block;
        list_for_each(block, &g_kernel_memory.list_of_free_memory[i])
        {
            blocks++;
        }
        printk("Buddy: order %zd, %zd blocks of %d KB free (total: %zd KB)\n",
               i, blocks, (1 << i) * 4, (1 << i) * 4 * blocks);
    }
}

void kalloc_debug_check_caches()
{
    for (size_t i = 0; i < OBJECT_CACHES; ++i)
    {
        kmem_cache_check(&g_kernel_memory.object_cache[i]);
    }
}
