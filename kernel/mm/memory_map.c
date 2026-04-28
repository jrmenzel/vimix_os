/* SPDX-License-Identifier: MIT */

#include <asm/pgtable-bits.h>
#include <drivers/devices_list.h>
#include <init/early_pgtable.h>
#include <init/start.h>
#include <kernel/pgtable.h>
#include <kernel/string.h>
#include <lib/minmax.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>
#include <mm/memory_map.h>

struct MM_Region_Attributes g_region_attributes[] = {
    [MM_REGION_RESERVED] = {.description = "reserved",
                            .pte_flags = 0,
                            .free_pages = false,
                            .copy_on_fork = false},
    [MM_REGION_EARLY_RAM] = {.description = "early RAM",
                             .pte_flags = PTE_RW_RAM,
                             .free_pages = false,
                             .copy_on_fork = false},
    [MM_REGION_USABLE_RAM] = {.description = "usable RAM",
                              .pte_flags = PTE_RW_RAM,
                              .free_pages = false,
                              .copy_on_fork = false},
    [MM_REGION_KERNEL] = {.description = "kernel",
                          .pte_flags = 0,
                          .free_pages = false,
                          .copy_on_fork = false},
    [MM_REGION_KERNEL_TEXT] = {.description = "kernel text",
                               .pte_flags = PTE_RO_TEXT,
                               .free_pages = false,
                               .copy_on_fork = false},
    [MM_REGION_KERNEL_TEXT_PA] = {.description = "kernel entry at PA",
                                  .pte_flags = PTE_RO_TEXT,
                                  .free_pages = false,
                                  .copy_on_fork = false},
    [MM_REGION_KERNEL_RO_DATA] = {.description = "kernel RO data",
                                  .pte_flags = PTE_R,
                                  .free_pages = false,
                                  .copy_on_fork = false},
    [MM_REGION_KERNEL_DATA] = {.description = "kernel data",
                               .pte_flags = PTE_RW_RAM,
                               .free_pages = false,
                               .copy_on_fork = false},
    [MM_REGION_KERNEL_BSS] = {.description = "kernel BSS",
                              .pte_flags = PTE_RW_RAM,
                              .free_pages = false,
                              .copy_on_fork = false},
    [MM_REGION_DTB] = {.description = "device tree blob",
                       .pte_flags = PTE_R,
                       .free_pages = false,
                       .copy_on_fork = false},
    [MM_REGION_INITRD] = {.description = "initial RAM disk",
                          .pte_flags = PTE_RW_RAM,
                          .free_pages = false,
                          .copy_on_fork = false},
    [MM_REGION_MMIO] = {.description = "memory-mapped I/O",
                        .pte_flags = PTE_MMIO_FLAGS,
                        .free_pages = false,
                        .copy_on_fork = false},
    [MM_REGION_USER_TEXT] = {.description = "user text",
                             .pte_flags = PTE_RO_TEXT | PTE_U,
                             .free_pages = true,
                             .copy_on_fork = true},
    [MM_REGION_USER_RW_TEXT] = {.description = "user read-write text",
                                .pte_flags = PTE_RO_TEXT | PTE_RW | PTE_U,
                                .free_pages = true,
                                .copy_on_fork = true},
    [MM_REGION_USER_RO_DATA] = {.description = "user read-only data",
                                .pte_flags = PTE_R | PTE_U,
                                .free_pages = true,
                                .copy_on_fork = true},
    [MM_REGION_USER_DATA] = {.description = "user data",
                             .pte_flags = PTE_USER_RAM,
                             .free_pages = true,
                             .copy_on_fork = true},
    [MM_REGION_USER_BSS] = {.description = "user BSS",
                            .pte_flags = PTE_USER_RAM,
                            .free_pages = true,
                            .copy_on_fork = true},
    [MM_REGION_USER_STACK] = {.description = "user stack",
                              .pte_flags = PTE_USER_RAM,
                              .free_pages = true,
                              .copy_on_fork = true},
    [MM_REGION_USER_KSTACK] = {.description = "user kernel stack",
                               .pte_flags = PTE_KERNEL_STACK,
                               .free_pages = true,
                               .copy_on_fork = false},
    [MM_REGION_USER_TRAPFRAME] = {.description = "user trapframe",
                                  .pte_flags = PTE_RW_RAM,
                                  .free_pages = false,
                                  .copy_on_fork = false},
    [MM_REGION_USER_TRAMPOLINE] = {.description = "user trampoline",
                                   .pte_flags = PTE_RO_TEXT,
                                   .free_pages = false,
                                   .copy_on_fork = false}};

void mm_region_init(struct MM_Region *region, size_t start_pa, size_t start_va,
                    size_t size, enum MM_Region_Type type)
{
    region->start_pa = start_pa;
    region->start_va = start_va;
    region->size = size;
    region->type = type;
    region->mapped =
        ((type == MM_REGION_RESERVED) || (type == MM_REGION_KERNEL))
            ? MM_REGION_NEVER_MAP
            : MM_REGION_MARKED_FOR_MAPPING;
    region->free_on_unmap = g_region_attributes[type].free_pages;

    DEBUG_EXTRA_PANIC(region->start_pa % PAGE_SIZE == 0, "unaligned region");
    DEBUG_EXTRA_PANIC(region->start_va % PAGE_SIZE == 0, "unaligned region");
    DEBUG_EXTRA_PANIC(region->size % PAGE_SIZE == 0, "unaligned size");

    list_init(&region->list);
}

struct MM_Region *mm_region_alloc_init(size_t start_pa, size_t start_va,
                                       size_t size, enum MM_Region_Type type)
{
    struct MM_Region *region =
        kmalloc(sizeof(struct MM_Region), ALLOC_FLAG_NONE);
    if (region == NULL)
    {
        return NULL;
    }
    mm_region_init(region, start_pa, start_va, size, type);
    return region;
}

bool mm_regions_can_be_merged(struct MM_Region *a, struct MM_Region *b)
{
    if (a->type != b->type) return false;

    // keep MMIO separate for easier debugging
    if (a->type == MM_REGION_MMIO) return false;

    if (a->mapped != b->mapped) return false;

    bool a_before_b_va = (a->start_va + a->size == b->start_va);
    bool b_before_a_va = (b->start_va + b->size == a->start_va);
    bool a_before_b_pa = (a->start_pa + a->size == b->start_pa);
    bool b_before_a_pa = (b->start_pa + b->size == a->start_pa);

    bool regions_touch =
        ((a_before_b_va && a_before_b_pa) || (b_before_a_va && b_before_a_pa));
    return regions_touch;
}

// true if a fits before b in virtual address space, and they don't overlap
static inline bool mm_region_fits_before(struct MM_Region *a,
                                         struct MM_Region *b)
{
    size_t a_end_va = a->start_va + a->size - 1;
    return (a_end_va < b->start_va);
}

static inline void mm_regions_merge(struct MM_Region *a, struct MM_Region *b)
{
    size_t merged_start_va = min(a->start_va, b->start_va);
    size_t merged_start_pa = min(a->start_pa, b->start_pa);
    size_t merged_end_va = max(a->start_va + a->size, b->start_va + b->size);
    a->start_va = merged_start_va;
    a->start_pa = merged_start_pa;
    a->size = merged_end_va - merged_start_va;

    kfree(b);
}

void memory_map_free(struct Memory_Map *map)
{
    struct list_head *pos, *tmp;
    list_for_each_safe(pos, tmp, &map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);
        mm_region_remove(region);
    }
}

void memory_map_set_ram(struct Memory_Map *map, size_t start_pa,
                        size_t start_va, size_t size)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);
    mm_region_init(&map->ram, start_pa, start_va, size, MM_REGION_USABLE_RAM);
    memory_map_add_region_and_split(map, start_pa, start_va, size,
                                    MM_REGION_USABLE_RAM);
}

void memory_map_copy_from_early_memory_map(struct Memory_Map *map,
                                           struct Early_Memory_Map *early_map)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);

    map->kernel = early_map->kernel;
    memory_map_add_region_and_split(
        map, early_map->kernel.start_pa, early_map->kernel.start_va,
        early_map->kernel.size, early_map->kernel.type);

    for (size_t i = 0; i < early_map->region_count; ++i)
    {
        memory_map_add_region_and_split(
            map, early_map->region[i].start_pa, early_map->region[i].start_va,
            early_map->region[i].size, early_map->region[i].type);
    }
}

void memory_map_add_single_region(struct Memory_Map *map,
                                  struct MM_Region *new_region)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);
    DEBUG_EXTRA_PANIC(new_region->start_pa % PAGE_SIZE == 0,
                      "unaligned region");
    DEBUG_EXTRA_PANIC(new_region->start_va % PAGE_SIZE == 0,
                      "unaligned region");
    DEBUG_EXTRA_PANIC(new_region->size % PAGE_SIZE == 0, "unaligned size");

    struct list_head *pos;
    list_for_each(pos, &map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);

        if (mm_regions_can_be_merged(region, new_region))
        {
            // merge regions by resizing old region to include new region and
            // deleting new region
            mm_regions_merge(region, new_region);
            return;
        }
        // new region fits before region -> add before
        if (mm_region_fits_before(new_region, region))
        {
            list_add(&new_region->list, pos->prev);
            return;
        }
    }
    list_add_tail(&new_region->list, &map->region_list);
}

void memory_map_add_single_region_and_split(struct Memory_Map *map,
                                            struct MM_Region *new_region)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);
    DEBUG_EXTRA_PANIC(new_region->start_pa % PAGE_SIZE == 0,
                      "unaligned region");
    DEBUG_EXTRA_PANIC(new_region->start_va % PAGE_SIZE == 0,
                      "unaligned region");
    DEBUG_EXTRA_PANIC(new_region->size % PAGE_SIZE == 0, "unaligned size");

    struct list_head *pos;
    list_for_each(pos, &map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);

        size_t new_region_end = new_region->start_va + new_region->size - 1;
        size_t region_end = region->start_va + region->size - 1;

        if (mm_regions_can_be_merged(region, new_region))
        {
            // merge regions by resizing old region to include new region and
            // deleting new region
            mm_regions_merge(region, new_region);
            return;
        }

        // new region replaces old one
        if ((new_region->start_va == region->start_va) &&
            (new_region->size == region->size))
        {
            list_add(&new_region->list, pos->prev);
            list_del(&region->list);
            kfree(region);
            return;
        }

        // new region fits before region -> add before
        if (mm_region_fits_before(new_region, region))
        {
            list_add(&new_region->list, pos->prev);
            return;
        }

        // new region fits inside of region, starts at region start -> add new
        // and resize old region
        if ((new_region->start_va == region->start_va) &&
            (new_region_end < region_end))
        {
            list_add(&new_region->list, pos->prev);
            // resize old region:
            region->start_va = new_region_end + 1;
            region->start_pa += new_region->size;
            region->size -= new_region->size;
            return;
        }

        // new region fits inside of region -> add new and split old region
        if ((new_region->start_va > region->start_va) &&
            (new_region_end < region_end))
        {
            // inserted behind region:
            struct MM_Region *split_region =
                kmalloc(sizeof(struct MM_Region), ALLOC_FLAG_NONE);
            if (split_region == NULL)
            {
                panic("memory_map_add_single_region_and_split: out of memory");
            }
            *split_region = *region;  // copy old region to split region

            // resize old region:
            region->size = new_region->start_va - region->start_va;
            DEBUG_EXTRA_PANIC(region->size % PAGE_SIZE == 0, "unaligned size");

            // add new region:
            list_add(&new_region->list, pos);

            // prepare split region:
            size_t offset = (new_region->size + region->size);
            split_region->start_va += offset;
            split_region->start_pa += offset;
            split_region->size -= offset;
            // add split region:
            if (split_region->size > 0)
            {
                list_add(&split_region->list, &new_region->list);
            }
            else
            {
                kfree(split_region);
            }
            return;
        }
    }
    list_add_tail(&new_region->list, &map->region_list);
}

void memory_map_add_region_and_split(struct Memory_Map *map, size_t start_pa,
                                     size_t start_va, size_t size,
                                     enum MM_Region_Type type)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);
    if (size == 0)
    {
        return;
    }

    struct MM_Region *region =
        mm_region_alloc_init(start_pa, start_va, size, type);
    if (region == NULL)
    {
        panic("memory_map_add_region_and_split: out of memory");
    }

    memory_map_add_single_region_and_split(map, region);
}

void memory_map_add_device_mmio(struct Memory_Map *map,
                                struct Devices_List *dev_list)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);
    // map all found MMIO devices
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &(dev_list->dev[i]);
        if (dev->init_parameters.mmu_map_memory)
        {
            for (size_t i = 0; i < DEVICE_MAX_MEM_MAPS; ++i)
            {
                // memory size of 0 means end of the list
                if (dev->init_parameters.mem[i].size == 0) break;

                size_t map_start_pa =
                    PAGE_ROUND_DOWN(dev->init_parameters.mem[i].start_pa);
                size_t map_end_pa =
                    PAGE_ROUND_UP(dev->init_parameters.mem[i].start_pa +
                                  dev->init_parameters.mem[i].size);
                size_t map_size = map_end_pa - map_start_pa;

                size_t offset = vm_get_mmio_offset(map_start_pa, map_size);
                dev->init_parameters.mem[i].start_va =
                    dev->init_parameters.mem[i].start_pa + offset;

                struct MM_Region *region =
                    mm_region_alloc_init(map_start_pa, map_start_pa + offset,
                                         map_size, MM_REGION_MMIO);
                if (region == NULL)
                {
                    panic("memory_map_add_device_mmio: out of memory");
                }

                memory_map_add_single_region(map, region);
            }
        }
    }
}

void mm_region_remove(struct MM_Region *region)
{
    if (region->free_on_unmap)
    {
        free_pages_range((void *)phys_to_virt(region->start_pa),
                         region->size / PAGE_SIZE);
    }
    list_del(&region->list);
    kfree(region);
}

void mm_region_resize(struct MM_Region *region, size_t new_start_va,
                      size_t new_size)
{
    if (new_size == 0)
    {
        mm_region_remove(region);
        return;
    }

    ssize_t start_offset = new_start_va - region->start_va;
    ssize_t end_offset =
        (new_start_va + new_size) - (region->start_va + region->size);

    if (region->free_on_unmap)
    {
        if (start_offset > 0)
        {
            free_pages_range((void *)phys_to_virt(region->start_pa),
                             start_offset / PAGE_SIZE);
        }
        if (end_offset < 0)
        {
            free_pages_range((void *)phys_to_virt(region->start_pa +
                                                  region->size + end_offset),
                             -end_offset / PAGE_SIZE);
        }
    }

    region->start_va = new_start_va;
    region->start_pa += start_offset;
    region->size = new_size;
}

void memory_map_remove_regions(struct Memory_Map *map, size_t start_va,
                               size_t size)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(map->parent_lock);
    DEBUG_EXTRA_PANIC(start_va % PAGE_SIZE == 0, "unaligned start_va");
    DEBUG_EXTRA_PANIC(size % PAGE_SIZE == 0, "unaligned size");

    size_t end_va = start_va + size - 1;
    struct list_head *pos, *tmp;
    list_for_each_safe(pos, tmp, &map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);
        size_t region_end_va = region->start_va + region->size - 1;

        // fits completely in delete range
        if ((region->start_va >= start_va) && (region_end_va <= end_va))
        {
            mm_region_remove(region);
        }
        // overlaps with delete range, but starts before delete range -> resize
        else if ((region->start_va < start_va) && (region_end_va > start_va) &&
                 (region_end_va <= end_va))
        {
            mm_region_resize(region, region->start_va,
                             start_va - region->start_va);
        }
        // overlaps with delete range, but ends after delete range -> resize
        else if ((region->start_va >= start_va) &&
                 (region->start_va < end_va) && (region_end_va >= end_va))
        {
            size_t offset = end_va - region->start_va + 1;
            mm_region_resize(region, region->start_va + offset,
                             region->size - offset);
        }
        // overlaps with delete range, but starts before and ends after delete
        // range -> split
        // we can assume this wont happen
        else if ((region->start_va < start_va) && (region_end_va > end_va))
        {
            panic(
                "memory_map_remove_regions: cannot handle splitting a region, "
                "not implemented");
        }
    }
}

void debug_print_mm_region(struct MM_Region *region)
{
    const char *type_str = g_region_attributes[region->type].description;

    printk("PA " FORMAT_REG_SIZE ", VA " FORMAT_REG_SIZE ", size %6zukb, %s",
           region->start_pa, region->start_va, region->size / 1024, type_str);

    switch (region->mapped)
    {
        case MM_REGION_NEVER_MAP: printk(", unmapped"); break;
        case MM_REGION_MARKED_FOR_MAPPING:
            printk(", marked for mapping");
            break;
        case MM_REGION_PARTIAL_MAPPED: printk(", partially mapped"); break;
        case MM_REGION_MAPPED: printk(", mapped"); break;
        case MM_REGION_MARKED_FOR_UNMAPPING:
            printk(", marked for unmapping");
            break;
    }
    printk("\n");
}

void debug_print_memory_map(struct Memory_Map *map)
{
    printk("Memory Map:\n");
    if (map->ram.size > 0)
    {
        printk("  RAM:    ");
        debug_print_mm_region(&map->ram);
    }
    if (map->kernel.size > 0)
    {
        printk("  Kernel: ");
        debug_print_mm_region(&map->kernel);
    }

    struct list_head *pos;
    list_for_each(pos, &map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);
        debug_print_mm_region(region);
    }
}
