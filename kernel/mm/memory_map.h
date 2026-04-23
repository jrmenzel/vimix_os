/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

enum Memory_Map_Region_Type
{
    MEM_MAP_REGION_UNUSED = 0,
    MEM_MAP_REGION_EARLY_RAM,
    MEM_MAP_REGION_USABLE_RAM,
    MEM_MAP_REGION_KERNEL,
    MEM_MAP_REGION_KERNEL_TEXT,
    MEM_MAP_REGION_KERNEL_RO_DATA,
    MEM_MAP_REGION_KERNEL_DATA,
    MEM_MAP_REGION_KERNEL_BSS,
    MEM_MAP_REGION_DTB,
    MEM_MAP_REGION_INITRD,
    MEM_MAP_REGION_RESERVED
};

struct Memory_Map_Region
{
    size_t start_pa;
    size_t start_va;
    size_t size;
    enum Memory_Map_Region_Type type;
    bool mapped;
};
#define MEM_MAP_MAX_REGIONS 16
struct Memory_Map
{
    struct Memory_Map_Region region[MEM_MAP_MAX_REGIONS];
    struct Memory_Map_Region ram;     // unsplit copy
    struct Memory_Map_Region kernel;  // unsplit copy
    size_t region_count;
};

void memory_map_init(struct Memory_Map *map);

void memory_map_add_region(struct Memory_Map *map, size_t start_pa,
                           size_t start_va, size_t size,
                           enum Memory_Map_Region_Type type);

void memory_map_copy_region(struct Memory_Map *map,
                            struct Memory_Map_Region *region);

void memory_map_sort(struct Memory_Map *map);

pte_t memory_map_get_pte(struct Memory_Map_Region *region);

// returns the first region of the given type, or NULL if not found
struct Memory_Map_Region *memory_map_get_region(
    struct Memory_Map *map, enum Memory_Map_Region_Type type);

void debug_print_memory_map(struct Memory_Map *map);
