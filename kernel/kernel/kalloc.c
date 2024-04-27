/* SPDX-License-Identifier: MIT */

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

void freerange(void *pa_start, void *pa_end);

extern char end_of_kernel[];  /// first address after kernel.
                              /// defined by kernel.ld.

/// each free 4KB page will contain this struct to form a linked list
struct run
{
    struct run *next;
};

/// a linked list of all free 4KB pages for the kernel to allocate
struct
{
    struct spinlock lock;
    struct run *freelist;
} kmem;

void kalloc_init()
{
    spin_lock_init(&kmem.lock, "kmem");
    freerange(end_of_kernel, (void *)PHYSTOP);
}

/// @brief helper for init_physical_page_allocator. Frees all pages in
/// a given range for the kernel to use.
/// @param pa_start physical address of the start
/// @param pa_end physical address of the end
void freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char *)PAGE_ROUND_UP((size_t)pa_start);
    for (; p + PAGE_SIZE <= (char *)pa_end; p += PAGE_SIZE)
    {
        kfree(p);
    }
}

void kfree(void *pa)
{
    struct run *r;

    if (((size_t)pa % PAGE_SIZE) != 0 || (char *)pa < end_of_kernel ||
        (size_t)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PAGE_SIZE);

    r = (struct run *)pa;

    spin_lock(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    spin_unlock(&kmem.lock);
}

void *kalloc()
{
    struct run *r;

    spin_lock(&kmem.lock);
    r = kmem.freelist;
    if (r) kmem.freelist = r->next;
    spin_unlock(&kmem.lock);

    if (r) memset((char *)r, 5, PAGE_SIZE);  // fill with junk
    return (void *)r;
}
