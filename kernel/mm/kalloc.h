/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <mm/vm.h>

#define ALLOC_FLAG_NONE 0
#define ALLOC_FLAG_ZERO_MEMORY 1

/// @brief Allocate a power of two number of pages
/// @param flags Returns zeroes memory if ALLOC_FLAG_ZERO_MEMORY is set.
/// @param order Number of pages to allocate as 2^order
/// @return Pointer to the allocation or NULL if out of
/// (enough continuous) memory.
void *alloc_pages(int32_t flags, size_t order);

/// @brief Allocate one page.
/// @param flags To zero or not
/// @return A page or NULL
static inline void *alloc_page(int32_t flags) { return alloc_pages(flags, 0); }

/// @brief Free an allocation done with alloc_pages
/// @param kva Pointer to allocation
/// @param order Same value as used during allocation
void free_pages(void *kva, size_t order);

/// @brief Free a range of consecutive pages allocated by alloc_pages. Order is
/// implicitly 0.
/// @param kva Start address
/// @param page_count Number of pages to free
void free_pages_range(void *kva, size_t page_count);

/// @brief Free one page allocated by alloc_page()
/// @param kva Address of page to free
static inline void free_page(void *kva) { free_pages(kva, 0); }

/// @brief Allocate up to one page of physical memory.
/// Use alloc_pages() when more is needed.
/// Returns a pointer that the kernel can use.
/// @param size Number of bytes to allocate, must be > 0 and <= PAGE_SIZE
/// @param flags Allocation flags, see ALLOC_FLAG_...
/// @return NULL if the memory cannot be allocated.
void *kmalloc(size_t size, int32_t flags);

/// @brief Free memory pointed at by pa,
/// which normally should have been returned by a
/// call to kalloc() or kmalloc().  (The exception is when
/// initializing the allocator; see kalloc_init())
/// @param kva kernel virtual address of the memory to free
void kfree(void *kva);

/// @brief Free the RAM of region MM_REGION_EARLY_RAM, called once at
/// boot.
/// @param region physical addresses of the memory to use (region
/// MM_REGION_EARLY_RAM)
void kalloc_init(struct MM_Region *region);

void kalloc_init_memory(struct Memory_Map *memory_map,
                        enum MM_Region_Type type);

/// Returns the number of 4K allocations currently used.
size_t kalloc_get_allocation_count();

/// @brief Returns the number of bytes allocated excluding
/// slab management overhead. This is intended for more comparable
/// values ignoring slab fragmentation. Used by user space test apps to detecct
/// kernel memory leaks.
size_t kalloc_get_memory_allocated();

/// @brief Returns total memory in bytes
size_t kalloc_get_total_memory();

/// @brief Returns free memory in bytes
size_t kalloc_get_free_memory();

void kalloc_dump_free_memory();

void kalloc_debug_check_caches();
