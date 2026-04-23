/* SPDX-License-Identifier: MIT */

#include <init/dtb.h>
#include <init/early_pgtable.h>
#include <init/start.h>
#include <kernel/kernel.h>
#include <kernel/page.h>
#include <kernel/pgtable.h>
#include <libfdt.h>
#include <mm/arch_early_pgtable.h>
#include <mm/memory_map.h>
#include <mm/vm.h>

__attribute__((
    aligned(PAGE_SIZE))) char g_early_kernel_page_table[EARLY_PAGE_TABLE_SIZE];

struct early_alloc_state
{
    size_t next_free_page;
    size_t free_page_count;
};

void early_panic() { infinite_loop; }

size_t early_vm_alloc_page(struct early_alloc_state *alloc_state)
{
    if (alloc_state->free_page_count == 0)
    {
        early_panic();
    }

    size_t page = alloc_state->next_free_page;
    alloc_state->next_free_page += PAGE_SIZE;
    alloc_state->free_page_count--;

    return page;
}

pagetable_t early_vm_walk(pagetable_t pagetable,
                          struct early_alloc_state *alloc_state, size_t va)
{
    for (size_t level = (PAGE_TABLE_MAX_LEVELS - 1); level > 0; level--)
    {
        size_t index = PAGE_TABLE_INDEX(level, va);
        pte_t pte = pagetable[index];

        if (!PTE_IS_VALID_NODE(pte))
        {
            // the path does not exist, create it
            size_t new_page_pa = early_vm_alloc_page(alloc_state);
            pagetable[index] = PTE_BUILD(new_page_pa, PTE_V);
        }
        pagetable = (pagetable_t)PTE_GET_PA(pagetable[index]);
    }

    return pagetable;
}

void early_vm_map(pagetable_t pagetable, struct early_alloc_state *alloc_state,
                  size_t va, size_t pa, size_t size, uint32_t flags)
{
    flags |= PTE_MAP_DEFAULT_FLAGS;

    if ((va % PAGE_SIZE) != 0 || (size % PAGE_SIZE) != 0)
    {
        early_panic();
    }

    for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
    {
        size_t va_offset = va + offset;
        size_t pa_offset = pa + offset;
        pagetable_t last_level =
            early_vm_walk(pagetable, alloc_state, va_offset);
        size_t index = PAGE_TABLE_INDEX(0, va_offset);
        last_level[index] = PTE_BUILD(pa_offset, flags);
    }
}

size_t early_pgtable_init(size_t pt_paddr, size_t dtb_paddr,
                          size_t memory_map_paddr)
{
    struct Memory_Map *memory_map = (struct Memory_Map *)memory_map_paddr;
    memory_map_init(memory_map);

    pagetable_t pagetable = (pagetable_t)(pt_paddr);

    struct early_alloc_state alloc_state;
    // first page is implicitly in use as the root page table
    alloc_state.next_free_page = pt_paddr + PAGE_SIZE;
    alloc_state.free_page_count = (EARLY_PT_RESERVED_PAGES)-1;

    // NOTE:
    // Before relocation, addresses materialized from symbols may resolve near
    // the current PC (physical load address). Therefore, do not free these
    // values through virt_to_phys()/phys_to_virt() here.
    //
    // Build explicit kernel PA/VA pairs using section offsets from
    // __start_kernel plus the configured PAGE_OFFSET/PHYS_OFFSET.
    const size_t kernel_start_any = (size_t)__start_kernel;
    const size_t kernel_start_pa = virt_to_phys(PAGE_OFFSET + TEXT_OFFSET);
    const size_t kernel_start_va = phys_to_virt(kernel_start_pa);

    const size_t rodata_off = (size_t)__start_rodata - kernel_start_any;
    const size_t data_off = (size_t)__start_data - kernel_start_any;
    const size_t bss_off = (size_t)__start_bss - kernel_start_any;
    const size_t kernel_end_off = (size_t)__end_of_kernel - kernel_start_any;

    const size_t kernel_rodata_va = kernel_start_va + rodata_off;
    const size_t kernel_rodata_pa = kernel_start_pa + rodata_off;
    const size_t kernel_data_va = kernel_start_va + data_off;
    const size_t kernel_data_pa = kernel_start_pa + data_off;
    const size_t kernel_bss_va = kernel_start_va + bss_off;
    const size_t kernel_bss_pa = kernel_start_pa + bss_off;
    const size_t kernel_end_pa = kernel_start_pa + kernel_end_off;

    memory_map->kernel.start_pa = kernel_start_pa;
    memory_map->kernel.start_va = kernel_start_va;
    memory_map->kernel.size = PAGE_ROUND_UP(kernel_end_off);
    memory_map->kernel.type = MEM_MAP_REGION_KERNEL;
    memory_map->kernel.mapped = false;

    // map first page of kernel text (init code) once with va=pa to be able to
    // fetch instructions after enabling this page table for the jump to the
    // higher VA.
    memory_map_add_region(memory_map, kernel_start_pa, kernel_start_pa,
                          PAGE_SIZE, MEM_MAP_REGION_KERNEL_TEXT);

    memory_map_add_region(memory_map, kernel_start_pa, kernel_start_va,
                          PAGE_ROUND_UP((size_t)__size_of_text),
                          MEM_MAP_REGION_KERNEL_TEXT);
    memory_map_add_region(memory_map, kernel_rodata_pa, kernel_rodata_va,
                          PAGE_ROUND_UP((size_t)__size_of_rodata),
                          MEM_MAP_REGION_KERNEL_RO_DATA);
    memory_map_add_region(memory_map, kernel_data_pa, kernel_data_va,
                          PAGE_ROUND_UP((size_t)__size_of_data),
                          MEM_MAP_REGION_KERNEL_DATA);
    memory_map_add_region(memory_map, kernel_bss_pa, kernel_bss_va,
                          PAGE_ROUND_UP((size_t)__size_of_bss),
                          MEM_MAP_REGION_KERNEL_BSS);

    // early RAM
    size_t ram_start_pa = PAGE_ROUND_UP(kernel_end_pa);
    size_t ram_end_pa = MEGA_PAGE_ROUND_UP(kernel_end_pa) + MEGA_PAGE_SIZE;

    memory_map_add_region(memory_map, ram_start_pa, phys_to_virt(ram_start_pa),
                          ram_end_pa - ram_start_pa, MEM_MAP_REGION_EARLY_RAM);

    // map DTB
    size_t dtb_page = PAGE_ROUND_DOWN(dtb_paddr);
    size_t dtb_size = fdt_totalsize(dtb_paddr);
    if (dtb_page < dtb_paddr)
    {
        // the dtb is not page aligned
        dtb_size += (dtb_paddr - dtb_page);
    }
    dtb_size = PAGE_ROUND_UP(dtb_size);

    memory_map_add_region(memory_map, dtb_page, phys_to_virt(dtb_page),
                          dtb_size, MEM_MAP_REGION_DTB);

    for (size_t i = 0; i < memory_map->region_count; ++i)
    {
        struct Memory_Map_Region *region = &memory_map->region[i];
        if (region->type == MEM_MAP_REGION_UNUSED)
        {
            continue;
        }
        uint32_t flags = memory_map_get_pte(region);
        if (flags == 0)
        {
            continue;
        }
        early_vm_map(pagetable, &alloc_state, region->start_va,
                     region->start_pa, region->size, flags);
        region->mapped = true;
    }

    return mmu_make_page_table_reg_pa(pt_paddr, 0);
}

void enable_early_pgtable(size_t pt_paddr)
{
    mmu_set_page_table_reg_value(mmu_make_page_table_reg_pa(pt_paddr, 0));
}