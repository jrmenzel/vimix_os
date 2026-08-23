/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>
#include <lib/bitmap.h>
#include <mm/memory_map.h>

/// @brief A page table, both the pgtable tree read by the MMU and
/// the Memory_Map struct which logs all mapped regions.
/// The same struct manages the kernels page table and user process page tables.
struct Page_Table
{
    pagetable_t root;
    struct spinlock lock;
    struct Memory_Map memory_map;
    atomic_size_t epoch;
    atomic_bool update_epoch_pending;
    limited_bitmap_t enabled_by_cpu;
};

_Static_assert(MAX_CPUS <= sizeof(limited_bitmap_t) * 8,
               "limited_bitmap_t is too small to track enabled_by_cpu");

/// @brief The one global kernel page table shared by all CPUs.
extern struct Page_Table *g_kernel_pagetable;

/// @brief Cacche of the last set MMU register value for additional cores to set
/// the page table ASAP.
extern size_t g_kernel_pgtable_reg_value;

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

syserr_t page_table_update_region_epoch(struct Page_Table *pagetable);

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
syserr_t page_table_unmap_remove_range(struct Page_Table *pagetable,
                                       size_t start_va, size_t size);

/// @brief Unmaps a region and removes it from the memory map.
/// @param pagetable Page table to unmap from.
/// @param region Region to unmap.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_unmap_remove_region(struct Page_Table *pagetable,
                                        struct MM_Region *region);

/// @brief Helper for fork: copy all mapped regions from src to dst which have
/// the copy_on_fork attribute, and apply the mapping.
/// @param dst Destination page table.
/// @param src Source page table.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_copy_on_fork(struct Page_Table *dst,
                                 struct Page_Table *src);

/// @brief The first time a user process runs on a CPU, the we must make sure
/// that no old fragments of code are still in the instruction cache. This is no
/// issue for switching user processes later as the cache is indexed by physical
/// address. BUT if process A ends and a new process B (by chance) gets the same
/// physical pages allocated for (some part) of its text sections, then the
/// instruction cache can have stale data. This olny happens on architectures
/// where data and instruction caches must be synced in software, e.g. ARM64. If
/// the kernel self modifies its code, it must also call this function.
/// @param pagetable The pagetable from which each region with code will be
/// synced.
/// @return 0 on success, or a negative error code on failure.
syserr_t page_table_sync_text_with_data(struct Page_Table *pagetable);
