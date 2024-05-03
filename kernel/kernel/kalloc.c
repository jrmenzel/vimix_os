/* SPDX-License-Identifier: MIT */

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

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

void kalloc_init(char *pa_start, char *pa_end)
{
    spin_lock_init(&g_kernel_memory.lock, "kmem");
    g_kernel_memory.end_of_physical_memory = pa_end;
    kfree_range(pa_start, pa_end);

#ifdef CONFIG_DEBUG_KALLOC
    // reset *after* kfree_range (as kfree decrements the counter)
    g_kernel_memory.pages_allocated = 0;
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
#endif  // CONFIG_DEBUG_KALLOC
    }
    spin_unlock(&g_kernel_memory.lock);

#ifdef CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
    if (page)
    {
        memset((char *)page, 5, PAGE_SIZE);  // fill with junk
    }
#endif  // CONFIG_DEBUG_KALLOC_MEMSET_KALLOC_FREE
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
