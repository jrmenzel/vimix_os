/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <mm/memory_map.h>

/// @brief A page table, both the pgtable tree read by the MMU and
/// the Memory_Map struct which logs all mapped regions.
/// The same struct manages the kernels page table and user process page tables.
struct Page_Table
{
    pagetable_t root;
    struct spinlock lock;
    struct Memory_Map memory_map;
};

/// @brief The one global kernel page table shared by all CPUs.
extern struct Page_Table *g_kernel_pagetable;

/// @brief Cacche of the last set MMU register value for additional cores to set
/// the page table ASAP.
extern size_t g_kernel_pagetable_register_value;

/// @brief Allocates and initializes a new page table. The returned page table
/// will have no mappings but one page for the pgtable root allocated.
/// @return A pointer to the new page table, or NULL on failure.
struct Page_Table *page_table_alloc_init();

/// @brief Frees a page table by freeing the memory map (which might free mapped
/// pages) and then free the pgtable tree.
/// @param pagetable Page table to free.
void page_table_free(struct Page_Table *pagetable);

/// @brief If new regions were added to the memory map, map them now. On failure
/// it will clear out all regions which were already mapped in this transaction
/// or marked to be mapped.
/// @param pagetable Page table with new/unmapped regions.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_apply_mapping(struct Page_Table *pagetable);

/// @brief Helper in page_table_apply_mapping and used in some
/// cleanup-after-failure code. Clear out any MM_REGION_PARTIAL_MAPPED mappings
/// or MM_REGION_MARKED_FOR_MAPPING regions.
/// @param pagetable Page table to clean up.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_unmap_partial_mappings(struct Page_Table *pagetable);

/// @brief Unmaps a range of virtual addresses and removes the area from the
/// memory map.
/// @param pagetable Page table to unmap from.
/// @param start_va Starting virtual address of the range to unmap.
/// @param size Size of the range to unmap in bytes.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_unmap_range(struct Page_Table *pagetable, size_t start_va,
                                size_t size);

/// @brief Unmaps a region and removes it from the memory map.
/// @param pagetable Page table to unmap from.
/// @param region Region to unmap.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_unmap_region(struct Page_Table *pagetable,
                                 struct MM_Region *region);

/// @brief Helper for fork: copy all mapped regions from src to dst which have
/// the copy_on_fork attribute, and apply the mapping.
/// @param dst Destination page table.
/// @param src Source page table.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_copy_on_fork(struct Page_Table *dst,
                                 struct Page_Table *src);
