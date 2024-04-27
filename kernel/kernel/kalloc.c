/* SPDX-License-Identifier: MIT */

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

void kfree_range(void *pa_start, void *pa_end);

extern char end_of_kernel[];  /// first address after kernel.
                              /// defined by kernel.ld.

/// each free 4KB page will contain this struct to form a linked list
struct free_page
{
    struct free_page *next;
};

/// a linked list of all free 4KB pages for the kernel to allocate
struct
{
    struct spinlock lock;
    struct free_page *free_list;
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
    char *p = (char *)PAGE_ROUND_UP((size_t)pa_start);
    for (; p + PAGE_SIZE <= (char *)pa_end; p += PAGE_SIZE)
    {
        kfree(p);
    }
}

void kalloc_init()
{
    spin_lock_init(&g_kernel_memory.lock, "kmem");
    kfree_range(end_of_kernel, (void *)PHYSTOP);

#ifdef CONFIG_DEBUG_KALLOC
    // reset *after* kfree_range (as kfree decrements the counter)
    g_kernel_memory.pages_allocated = 0;
#endif  // CONFIG_DEBUG_KALLOC
}

void kfree(void *pa)
{
    struct free_page *r;

    if (((size_t)pa % PAGE_SIZE) != 0 || (char *)pa < end_of_kernel ||
        (size_t)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PAGE_SIZE);

    r = (struct free_page *)pa;

    spin_lock(&g_kernel_memory.lock);
    r->next = g_kernel_memory.free_list;
    g_kernel_memory.free_list = r;
#ifdef CONFIG_DEBUG_KALLOC
    g_kernel_memory.pages_allocated--;
#endif  // CONFIG_DEBUG_KALLOC
    spin_unlock(&g_kernel_memory.lock);
}

void *kalloc()
{
    spin_lock(&g_kernel_memory.lock);
    struct free_page *r = g_kernel_memory.free_list;
    if (r)
    {
        g_kernel_memory.free_list = r->next;
#ifdef CONFIG_DEBUG_KALLOC
        g_kernel_memory.pages_allocated++;
#endif  // CONFIG_DEBUG_KALLOC
    }
    spin_unlock(&g_kernel_memory.lock);

    if (r)
    {
        memset((char *)r, 5, PAGE_SIZE);  // fill with junk
    }
    return (void *)r;
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
