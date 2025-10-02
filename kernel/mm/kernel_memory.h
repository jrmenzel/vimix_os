/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/kobject.h>
#include <kernel/list.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>
#include <mm/cache.h>
#include <mm/kernel_memory.h>

// The kernel saves free lists for 2^0 to 2^PAGE_ALLOC_MAX_ORDER pages
// it uses a buddy allocator strategy to merge free buddy blocks of
// order X to one of order X+1 and splits higher order blocks if no free
// blocks of the requested order are available.
#define PAGE_ALLOC_MAX_ORDER 9

// Number of Slab Allocator caches to provide allocations from
// MIN_SLAB_SIZE_ORDER to PAGE_SIZE/4
#define OBJECT_CACHES_POT ((PAGE_SHIFT - MIN_SLAB_SIZE_ORDER - 1))

// +1 to include 1280 byte cache (useful for buffer IO caches)
#define OBJECT_CACHES (OBJECT_CACHES_POT + 1)

struct kernel_memory
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
};

extern struct kernel_memory g_kernel_memory;
