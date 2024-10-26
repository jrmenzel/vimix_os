/* SPDX-License-Identifier: MIT */

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include <init/main.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <lib/minmax.h>

/// each free 4KB page will contain this struct to form a linked list
struct free_page
{
    struct free_page *next;
};

/// a linked list of all free 4KB pages for the kernel to allocate
struct
{
    struct spinlock lock;
    char *end_of_physical_memory;
    struct free_page *list_of_free_pages;
#ifdef CONFIG_DEBUG_KALLOC
    size_t pages_allocated;
    size_t pages_allocated_total;
#endif  // CONFIG_DEBUG_KALLOC
} g_kernel_memory;

/// @brief helper for kalloc_init. Frees all pages in
/// a given range for the kernel to use.
/// @param pa_start physical address of the start
/// @param pa_end physical address of the end
void kfree_range(void *pa_start, void *pa_end)
{
    char *first_free_page = (char *)PAGE_ROUND_UP((size_t)pa_start);
    for (char *page_address = first_free_page;
         page_address + PAGE_SIZE <= (char *)pa_end; page_address += PAGE_SIZE)
    {
        kfree(page_address);
    }
}

void kalloc_init(struct Minimal_Memory_Map *memory_map)
{
    spin_lock_init(&g_kernel_memory.lock, "kmem");
    g_kernel_memory.end_of_physical_memory = (char *)memory_map->ram_end;

    // The available memory after kernel_end can have up to two holes:
    // the dtb file and a initrd ramdisk, both are optional.
    // The dtb could also be outside of the RAM area (e.g. in flash).

    size_t region_start = memory_map->kernel_end;
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
        kfree_range((void *)region_start, (void *)region_end);
        if (region_end == memory_map->ram_end) break;

        region_start = next_region_start;
    }

#ifdef CONFIG_DEBUG_KALLOC
    // reset *after* kfree_range (as kfree decrements the counter)
    g_kernel_memory.pages_allocated = 0;
    g_kernel_memory.pages_allocated_total = 0;
#endif  // CONFIG_DEBUG_KALLOC
}

void kfree(void *pa)
{
    if (((size_t)pa % PAGE_SIZE) != 0  // if pa is not page aligned
        || (char *)pa < end_of_kernel  // or pa is before or inside the kernel
                                       // binary in memory
        || (char *)pa >=
               g_kernel_memory.end_of_physical_memory)  // or pa is outside of
                                                        // the physical memory
    {
        panic("kfree: out of range or unaligned address");
    }

#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PAGE_SIZE);
#endif  // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE

    struct free_page *page = (struct free_page *)pa;

    // add page to the start of the list of free pages:
    spin_lock(&g_kernel_memory.lock);
    page->next = g_kernel_memory.list_of_free_pages;
    g_kernel_memory.list_of_free_pages = page;
#ifdef CONFIG_DEBUG_KALLOC
    g_kernel_memory.pages_allocated--;
#endif  // CONFIG_DEBUG_KALLOC
    spin_unlock(&g_kernel_memory.lock);
}

void *kalloc()
{
    spin_lock(&g_kernel_memory.lock);
    struct free_page *page = g_kernel_memory.list_of_free_pages;
    if (page)
    {
        g_kernel_memory.list_of_free_pages = page->next;
#ifdef CONFIG_DEBUG_KALLOC
        g_kernel_memory.pages_allocated++;
        g_kernel_memory.pages_allocated_total++;
#endif  // CONFIG_DEBUG_KALLOC
    }
    spin_unlock(&g_kernel_memory.lock);

#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
    if (page)
    {
        memset((char *)page, 5, PAGE_SIZE);  // fill with junk
    }
#endif  // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
    // printk("allocated a page 0x%zx - used %zd - total: %zd\n", (size_t)page,
    //        g_kernel_memory.pages_allocated,
    //        g_kernel_memory.pages_allocated_total);
    // if (page == NULL)
    // {
    //     printk("WARNING: OUT OF MEMORY\n");
    // }

    return (void *)page;
}

#ifdef CONFIG_DEBUG_KALLOC
size_t kalloc_debug_get_allocation_count()
{
    spin_lock(&g_kernel_memory.lock);
    size_t count = g_kernel_memory.pages_allocated;
    spin_unlock(&g_kernel_memory.lock);
    return count;
}
#endif  // CONFIG_DEBUG_KALLOC

size_t kalloc_get_free_memory()
{
    spin_lock(&g_kernel_memory.lock);
    size_t pages = 0;

    struct free_page *mem = g_kernel_memory.list_of_free_pages;
    while (mem)
    {
        mem = mem->next;
        pages++;
    }

    spin_unlock(&g_kernel_memory.lock);

    return pages * PAGE_SIZE;
}
