/* SPDX-License-Identifier: MIT */

#include <asm/pgtable-bits.h>
#include <init/early_pgtable.h>
#include <init/start.h>
#include <kernel/pgtable.h>
#include <kernel/string.h>
#include <lib/minmax.h>
#include <mm/memory_map.h>

void memory_map_init(struct Memory_Map *map)
{
    map->region_count = 0;

    if (TEXT_OFFSET != 0)
    {
        memory_map_add_region(map, virt_to_phys(PAGE_OFFSET), PAGE_OFFSET,
                              TEXT_OFFSET, MEM_MAP_REGION_RESERVED);
    }
}

void memory_map_add_region(struct Memory_Map *map, size_t start_pa,
                           size_t start_va, size_t size,
                           enum Memory_Map_Region_Type type)
{
    if (map->region_count >= MEM_MAP_MAX_REGIONS)
    {
        panic("memory_map_add_region: too many regions");
    }
    if (size == 0)
    {
        return;
    }

    size_t idx = map->region_count;
    map->region_count++;

    size = PAGE_ROUND_UP(size);

    map->region[idx].start_pa = start_pa;
    map->region[idx].start_va = start_va;
    map->region[idx].size = size;
    map->region[idx].type = type;
    map->region[idx].mapped = false;
}

void memory_map_copy_region(struct Memory_Map *map,
                            struct Memory_Map_Region *region)
{
    memory_map_add_region(map, region->start_pa, region->start_va, region->size,
                          region->type);
    map->region[map->region_count - 1].mapped = region->mapped;
}

// returns the index of the region with the lowest start_pa, or -1 if no region
ssize_t memory_map_get_first_pa(struct Memory_Map *map)
{
    size_t first_pa = 0;
    ssize_t first_idx = -1;
    for (size_t i = 0; i < map->region_count; ++i)
    {
        if (map->region[i].type == MEM_MAP_REGION_UNUSED) continue;

        if (map->region[i].start_pa < first_pa || first_idx == -1)
        {
            first_pa = map->region[i].start_pa;
            first_idx = i;
        }
    }
    return first_idx;
}

void memory_map_sort(struct Memory_Map *map)
{
    struct Memory_Map tmp = *map;
    memset(map, 0, sizeof(struct Memory_Map));

    map->ram = tmp.ram;
    map->kernel = tmp.kernel;

    size_t next_free_addr = 0;
    while (true)
    {
        ssize_t idx = memory_map_get_first_pa(&tmp);
        if (idx == -1) break;

        if (next_free_addr < tmp.region[idx].start_pa)
        {
            // there is a gap, fill it with a kernel region or RAM region if the
            // RAM covers the gap:
            size_t gap_start = next_free_addr;
            size_t gap_end = tmp.region[idx].start_pa;

            size_t kernel_gap_start = max(tmp.kernel.start_pa, gap_start);
            size_t kernel_gap_end =
                min(tmp.kernel.start_pa + tmp.kernel.size, gap_end);
            if (kernel_gap_start < kernel_gap_end)
            {
                memory_map_add_region(
                    map, kernel_gap_start, phys_to_virt(kernel_gap_start),
                    kernel_gap_end - kernel_gap_start, MEM_MAP_REGION_KERNEL);

                next_free_addr = kernel_gap_end;
            }
            else
            {
                size_t ram_gap_start = max(tmp.ram.start_pa, gap_start);
                size_t ram_gap_end =
                    min(tmp.ram.start_pa + tmp.ram.size, gap_end);

                if (ram_gap_start < ram_gap_end)
                {
                    memory_map_add_region(
                        map, ram_gap_start, phys_to_virt(ram_gap_start),
                        ram_gap_end - ram_gap_start, MEM_MAP_REGION_USABLE_RAM);

                    next_free_addr = ram_gap_end;
                }
            }
        }

        // copy region to new map:
        memory_map_copy_region(map, &tmp.region[idx]);

        next_free_addr = tmp.region[idx].start_pa + tmp.region[idx].size;
        tmp.region[idx].type = MEM_MAP_REGION_UNUSED;
    }
    if (next_free_addr < tmp.ram.start_pa + tmp.ram.size)
    {
        // there is a gap at the end, fill it with a RAM region if the RAM
        // covers the gap:
        size_t gap_start = next_free_addr;
        size_t gap_end = tmp.ram.start_pa + tmp.ram.size;

        memory_map_add_region(map, gap_start, phys_to_virt(gap_start),
                              gap_end - gap_start, MEM_MAP_REGION_USABLE_RAM);
    }
}

pte_t memory_map_get_pte(struct Memory_Map_Region *region)
{
    pte_t flags = 0;
    switch (region->type)
    {
        case MEM_MAP_REGION_UNUSED: flags = 0; break;
        case MEM_MAP_REGION_EARLY_RAM: flags = PTE_RW_RAM; break;
        case MEM_MAP_REGION_USABLE_RAM: flags = PTE_RW_RAM; break;
        case MEM_MAP_REGION_KERNEL: flags = 0; break;
        case MEM_MAP_REGION_KERNEL_TEXT: flags = PTE_RO_TEXT; break;
        case MEM_MAP_REGION_KERNEL_RO_DATA: flags = PTE_R; break;
        case MEM_MAP_REGION_KERNEL_DATA: flags = PTE_RW_RAM; break;
        case MEM_MAP_REGION_KERNEL_BSS: flags = PTE_RW_RAM; break;
        case MEM_MAP_REGION_DTB: flags = PTE_R; break;
        case MEM_MAP_REGION_INITRD: flags = PTE_RW_RAM; break;
        case MEM_MAP_REGION_RESERVED: flags = 0; break;
    }
    return flags;
}

struct Memory_Map_Region *memory_map_get_region(
    struct Memory_Map *map, enum Memory_Map_Region_Type type)
{
    for (size_t i = 0; i < map->region_count; ++i)
    {
        if (map->region[i].type == type)
        {
            return &map->region[i];
        }
    }
    return NULL;
}

void debug_print_memory_region(struct Memory_Map_Region *region)
{
    const char *type_str = "unknown";
    switch (region->type)
    {
        case MEM_MAP_REGION_UNUSED: type_str = "unused"; break;
        case MEM_MAP_REGION_EARLY_RAM: type_str = "early RAM"; break;
        case MEM_MAP_REGION_USABLE_RAM: type_str = "usable RAM"; break;
        case MEM_MAP_REGION_KERNEL: type_str = "kernel"; break;
        case MEM_MAP_REGION_KERNEL_TEXT: type_str = "kernel text"; break;
        case MEM_MAP_REGION_KERNEL_RO_DATA: type_str = "kernel ro data"; break;
        case MEM_MAP_REGION_KERNEL_DATA: type_str = "kernel data"; break;
        case MEM_MAP_REGION_KERNEL_BSS: type_str = "kernel bss"; break;
        case MEM_MAP_REGION_DTB: type_str = "dtb"; break;
        case MEM_MAP_REGION_INITRD: type_str = "initrd"; break;
        case MEM_MAP_REGION_RESERVED: type_str = "reserved"; break;
    }
    printk("PA 0x%08zx, VA 0x%08zx, size 0x%08zx, %s, %s\n", region->start_pa,
           region->start_va, region->size,
           region->mapped ? "  mapped" : "unmapped", type_str);
}

void debug_print_memory_map(struct Memory_Map *map)
{
    printk("Memory Map:\n");
    printk("  RAM: ");
    debug_print_memory_region(&map->ram);
    printk("  Kernel: ");
    debug_print_memory_region(&map->kernel);

    for (size_t i = 0; i < map->region_count; ++i)
    {
        if (map->region[i].type == MEM_MAP_REGION_UNUSED) continue;

        printk("  Region %zu: ", i);
        debug_print_memory_region(&map->region[i]);
    }
}
