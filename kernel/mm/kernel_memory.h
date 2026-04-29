/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
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
    size_t phys_base;
    char *end_of_physical_memory;

    struct list_head list_of_free_memory[PAGE_ALLOC_MAX_ORDER + 1];

    struct kmem_cache object_cache[OBJECT_CACHES];

    atomic_size_t pages_allocated;
    struct Memory_Map *memory_map;
};

#define kernel_memory_from_kobj(kobj_ptr) \
    container_of(kobj_ptr, struct kernel_memory, kobj)

extern struct kernel_memory g_kernel_memory;

static inline bool addr_is_in_ram_pa(size_t pa)
{
    struct Memory_Map *map = g_kernel_memory.memory_map;
    if (map == NULL) return true;

    size_t ram_start = map->ram.start_pa;
    size_t ram_end = map->ram.start_pa + map->ram.size;
    return (pa >= ram_start && pa < ram_end);
}

static inline bool addr_is_in_ram_kva(size_t kva)
{
    struct Memory_Map *map = g_kernel_memory.memory_map;
    if (map == NULL) return true;

    size_t ram_start = map->ram.start_va;
    size_t ram_end = map->ram.start_va + map->ram.size;
    return (kva >= ram_start && kva < ram_end);
}
