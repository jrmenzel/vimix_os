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

struct run
{
    struct run *next;
};

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

void freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char *)PAGE_ROUND_UP((uint64)pa_start);
    for (; p + PAGE_SIZE <= (char *)pa_end; p += PAGE_SIZE) kfree(p);
}

/// Free the page of physical memory pointed at by pa,
/// which normally should have been returned by a
/// call to kalloc().  (The exception is when
/// initializing the allocator; see kalloc_init above.)
void kfree(void *pa)
{
    struct run *r;

    if (((uint64)pa % PAGE_SIZE) != 0 || (char *)pa < end_of_kernel ||
        (uint64)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PAGE_SIZE);

    r = (struct run *)pa;

    spin_lock(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    spin_unlock(&kmem.lock);
}

/// Allocate one 4096-byte page of physical memory.
/// Returns a pointer that the kernel can use.
/// Returns 0 if the memory cannot be allocated.
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
