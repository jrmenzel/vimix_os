/* SPDX-License-Identifier: MIT */
#pragma once

#include <asm/pgtable-bits.h>
#include <kernel/container_of.h>
#include <kernel/kernel.h>
#include <kernel/list.h>
#include <kernel/spinlock.h>

enum MM_Region_Type
{
    MM_REGION_RESERVED = 0,
    MM_REGION_EARLY_RAM,
    MM_REGION_USABLE_RAM,
    MM_REGION_KERNEL,
    MM_REGION_KERNEL_TEXT,
    MM_REGION_KERNEL_TEXT_PA,
    MM_REGION_KERNEL_RO_DATA,
    MM_REGION_KERNEL_DATA,
    MM_REGION_KERNEL_BSS,
    MM_REGION_DTB,
    MM_REGION_INITRD,
    MM_REGION_MMIO,
    MM_REGION_USER_TEXT,
    MM_REGION_USER_RW_TEXT,
    MM_REGION_USER_RO_DATA,
    MM_REGION_USER_DATA,
    MM_REGION_USER_BSS,
    MM_REGION_USER_STACK,
    MM_REGION_USER_KSTACK,
    MM_REGION_USER_TRAPFRAME,
    MM_REGION_USER_TRAMPOLINE
};

struct MM_Region_Attributes
{
    const char *description;
    pte_t pte_flags;
    bool free_pages;
    bool copy_on_fork;
};
extern struct MM_Region_Attributes g_region_attributes[];

/// @brief States of a region in the memory map.
enum MM_Region_Mapped
{
    MM_REGION_NEVER_MAP = 0,
    MM_REGION_MARKED_FOR_MAPPING,
    MM_REGION_PARTIAL_MAPPED,
    MM_REGION_MAPPED,
    MM_REGION_MARKED_FOR_UNMAPPING
};

struct MM_Region
{
    struct list_head list;
    size_t start_pa;
    size_t start_va;
    size_t size;
    enum MM_Region_Type type;
    enum MM_Region_Mapped mapped;
    bool free_on_unmap;
};

#define region_from_list(list_ptr) \
    container_of(list_ptr, struct MM_Region, list)

struct MM_Region *mm_region_alloc_init(size_t start_pa, size_t start_va,
                                       size_t size, enum MM_Region_Type type);

/// @brief Returns how the regions should get mapped based on its type.
/// @param region The region to get the pte flags for.
/// @return PTE for mapping of this region.
static inline pte_t mm_region_get_pte(struct MM_Region *region)
{
    return g_region_attributes[region->type].pte_flags;
}

/// @brief Removes a region from the memory map and frees the region.
/// @param region The region to remove and free.
void mm_region_remove(struct MM_Region *region);

void debug_print_mm_region(struct MM_Region *region);

/// @brief Central struct for the Page_Table to track the known memory regions.
struct Memory_Map
{
    struct MM_Region ram;     // unsplit copy
    struct MM_Region kernel;  // unsplit copy

    struct list_head region_list;

#ifdef CONFIG_DEBUG_SPINLOCK
    struct spinlock *parent_lock;
#endif
};

static inline void memory_map_init(struct Memory_Map *map)
{
    list_init(&map->region_list);
}

void memory_map_free(struct Memory_Map *map);

/// @brief Only relevant for the kernel page table.
/// @param map Kernel page table.
/// @param start_pa Physical address of the start of the RAM.
/// @param start_va Virtual address of the start of the RAM.
/// @param size Size of the RAM in bytes.
void memory_map_set_ram(struct Memory_Map *map, size_t start_pa,
                        size_t start_va, size_t size);

struct Early_Memory_Map;

/// @brief Only relevant for the kernel page table. Copy regions from the early
/// memory map.
/// @param map Destination kernel page table.
/// @param early_map Early memory map to copy regions from.
void memory_map_copy_from_early_memory_map(struct Memory_Map *map,
                                           struct Early_Memory_Map *early_map);

/// @brief Add a memory region. If it intersects existing regions, it will
/// resize the existing ones. Used when building the kernel page table: Add the
/// RAM region, then a full kernel map, then all smaller ones.
/// @param map Kernel page table.
/// @param start_pa Physical address of the start of the region.
/// @param start_va Virtual address of the start of the region.
/// @param size Size of the region in bytes.
/// @param type Type of the memory region.
void memory_map_add_region_and_split(struct Memory_Map *map, size_t start_pa,
                                     size_t start_va, size_t size,
                                     enum MM_Region_Type type);

/// @brief Add a new region to the map. Might merge with existing regions but
/// otherwise assumes there is space.
/// @param map Memory map.
/// @param new_region Region to add, memory map will take ownership.
void memory_map_add_single_region(struct Memory_Map *map,
                                  struct MM_Region *new_region);

struct Devices_List;
/// @brief Helper during early init to add MMIO regions for all found devices.
/// @param map Kernel memory map.
/// @param dev_list List of devices.
void memory_map_add_device_mmio(struct Memory_Map *map,
                                struct Devices_List *dev_list);

/// @brief Remove all memory regions in a given address range. Might resize
/// existing ones.
/// @param map Memory map to remove regions from.
/// @param start_va Start virtual address of the range to remove regions from.
/// @param size Size of the range to remove regions from in bytes (but assumed
/// to be page-aligned).
void memory_map_remove_regions(struct Memory_Map *map, size_t start_va,
                               size_t size);

/// @brief Debug dump.
/// @param map Memory map to print.
void debug_print_memory_map(struct Memory_Map *map);
