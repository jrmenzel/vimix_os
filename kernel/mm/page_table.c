/* SPDX-License-Identifier: MIT */

#include <kernel/pgtable.h>
#include <kernel/string.h>
#include <mm/kalloc.h>
#include <mm/page_table.h>
#include <mm/vm.h>

struct Page_Table *g_kernel_pagetable = NULL;
size_t g_kernel_pagetable_register_value;

syserr_t page_table_copy_from_region(struct Page_Table *dst_pagetable,
                                     struct Page_Table *src_pagetable,
                                     struct MM_Region *src_region);

struct Page_Table *page_table_alloc_init()
{
    struct Page_Table *pagetable = (struct Page_Table *)kmalloc(
        sizeof(struct Page_Table), ALLOC_FLAG_ZERO_MEMORY);
    if (pagetable == NULL)
    {
        return NULL;
    }

    pagetable->root = (pagetable_t)alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (pagetable->root == NULL)
    {
        kfree(pagetable);
        return NULL;
    }
    spin_lock_init(&pagetable->lock, "pagetable_lock");
    spin_lock(&pagetable->lock);
    memory_map_init(&pagetable->memory_map);

#ifdef CONFIG_DEBUG_SPINLOCK
    // for additional locking tests
    pagetable->memory_map.parent_lock = &pagetable->lock;
#endif

    return pagetable;
}

void page_table_free(struct Page_Table *pagetable)
{
    memory_map_free(&pagetable->memory_map);
    if (pagetable->root != NULL)
    {
        vm_free_pgtable(pagetable->root);
    }
    if (pagetable->lock.locked)
    {
        spin_unlock(&pagetable->lock);
    }
    kfree(pagetable);
}

syserr_t page_table_map_region(struct Page_Table *pagetable,
                               struct MM_Region *region)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);

    pte_t flags = mm_region_get_pte(region);
    if (flags == 0) return 0;  // ignore

    if (vm_map(pagetable, region->start_va, region->start_pa, region->size,
               flags, false) != 0)
    {
        return -ENOMEM;
    }
    region->mapped = MM_REGION_PARTIAL_MAPPED;

    return 0;
}

syserr_t page_table_apply_mapping(struct Page_Table *pagetable)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);

    struct Memory_Map *memory_map = &pagetable->memory_map;

    syserr_t err = 0;
    struct list_head *pos;
    list_for_each(pos, &memory_map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);
        if (region->mapped == MM_REGION_MARKED_FOR_MAPPING)
        {
            err = page_table_map_region(pagetable, region);
        }
        if (err < 0) break;
    }
    if (err >= 0)
    {
        // finalize mappings
        list_for_each(pos, &memory_map->region_list)
        {
            struct MM_Region *region = region_from_list(pos);
            if (region->mapped == MM_REGION_PARTIAL_MAPPED)
            {
                region->mapped = MM_REGION_MAPPED;
            }
        }
    }
    else
    {
        // some mappings failed, remove all regions of this transactions which
        // are now in state MM_REGION_PARTIAL_MAPPED
        page_table_unmap_partial_mappings(pagetable);
    }

    return err;
}

syserr_t page_table_unmap_partial_mappings(struct Page_Table *pagetable)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);

    struct Memory_Map *memory_map = &pagetable->memory_map;

    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &memory_map->region_list)
    {
        struct MM_Region *region = region_from_list(pos);
        if (region->mapped == MM_REGION_PARTIAL_MAPPED)
        {
            page_table_unmap_region(pagetable, region);
            mm_region_remove(region);
        }
        else if (region->mapped == MM_REGION_MARKED_FOR_MAPPING)
        {
            mm_region_remove(region);
        }
    }
    return 0;
}

syserr_t page_table_unmap_range(struct Page_Table *pagetable, size_t start_va,
                                size_t size)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);

    // note: unmap of 0 pages is fine!
    vm_unmap(pagetable, start_va, size / PAGE_SIZE, false);

    memory_map_remove_regions(&pagetable->memory_map, start_va, size);
    return 0;
}

syserr_t page_table_unmap_region(struct Page_Table *pagetable,
                                 struct MM_Region *region)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);

    vm_unmap(pagetable, region->start_va, region->size / PAGE_SIZE, false);

    mm_region_remove(region);
    return 0;
}

syserr_t page_table_copy_on_fork(struct Page_Table *dst, struct Page_Table *src)
{
    spin_lock(&dst->lock);
    spin_lock(&src->lock);

    syserr_t err = 0;
    struct list_head *pos;
    list_for_each(pos, &src->memory_map.region_list)
    {
        struct MM_Region *region = region_from_list(pos);
        if (region->mapped != MM_REGION_MAPPED) continue;
        if (g_region_attributes[region->type].copy_on_fork == false) continue;

        err = page_table_copy_from_region(dst, src, region);
        if (err < 0) break;
    }

    if (err < 0)
    {
        // free any regions we may have copied before the failure
        memory_map_free(&dst->memory_map);
        spin_unlock(&dst->lock);
        spin_unlock(&src->lock);
        return err;
    }

    err = page_table_apply_mapping(dst);
    spin_unlock(&dst->lock);
    spin_unlock(&src->lock);
    return err;
}

size_t page_table_get_kvm_addr(struct Page_Table *pagetable, size_t va)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);

    pte_t *pte = vm_walk(pagetable, va, false);
    if (pte == NULL)
    {
        panic("get_kvm_addr: pte should exist");
    }
    if (PTE_IS_VALID_NODE(*pte) == false)
    {
        panic("get_kvm_addr: page not present");
    }
    pte_t pa = PTE_GET_PA(*pte);
    return phys_to_virt(pa);
}

syserr_t page_table_copy_from_region(struct Page_Table *dst_pagetable,
                                     struct Page_Table *src_pagetable,
                                     struct MM_Region *src_region)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&src_pagetable->lock);

    struct Memory_Map *map = &dst_pagetable->memory_map;
    syserr_t err = 0;
    size_t offset = 0;
    for (; offset < src_region->size; offset += PAGE_SIZE)
    {
        char *mem = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
        if (mem == NULL)
        {
            err = -ENOMEM;
            break;
        }

        size_t va = src_region->start_va + offset;

        // copy page content from old region to new page
        size_t kvm_addr = page_table_get_kvm_addr(src_pagetable, va);
        memcpy(mem, (void *)kvm_addr, PAGE_SIZE);

        struct MM_Region *new_region = mm_region_alloc_init(
            virt_to_phys((size_t)mem), va, PAGE_SIZE, src_region->type);
        if (new_region == NULL)
        {
            free_page(mem);
            err = -ENOMEM;
            break;
        }
        memory_map_add_single_region(map, new_region);
    }

    if (err < 0)
    {
        // clean up any copied pages if we failed partway through
        memory_map_remove_regions(map, src_region->start_va, offset);
    }

    return err;
}
