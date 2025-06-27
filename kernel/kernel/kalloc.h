/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/vm.h>

#define ALLOC_FLAG_NONE 0
#define ALLOC_FLAG_ZERO_MEMORY 1

/// @brief Allocate a power of two number of pages
/// @param flags Returns zeroes memory if ALLOC_FLAG_ZERO_MEMORY is set.
/// @param order Number of pages to allocate as 2^order
/// @return Pointer to the allocation or NULL if out of
/// (enough continuous) memory.
void* alloc_pages(int32_t flags, size_t order);

/// @brief Allocate one page.
/// @param flags To zero or not
/// @return A page or NULL
static inline void* alloc_page(int32_t flags) { return alloc_pages(flags, 0); }

/// @brief Free an allocation done with alloc_pages
/// @param pa Pointer to allocation
/// @param order Same value as used during allocation
void free_pages(void* pa, size_t order);

/// @brief Free one page allocated by alloc_page()
/// @param pa Address of page to free
static inline void free_page(void* pa) { free_pages(pa, 0); }

/// @brief Allocate up to one page of physical memory.
/// Use alloc_pages() when more is needed.
/// Returns a pointer that the kernel can use.
/// @return NULL if the memory cannot be allocated.
void* kmalloc(size_t size);

/// @brief Free the page of physical memory pointed at by pa,
/// which normally should have been returned by a
/// call to kalloc().  (The exception is when
/// initializing the allocator; see kalloc_init())
/// @param pa physical address of the page to free
void kfree(void* pa);

/// @brief Free the RAM after the kernel loaded, called once at
/// boot.
/// @param memory_map physical addresses of the memory to use
void kalloc_init(struct Minimal_Memory_Map* memory_map);

#ifdef CONFIG_DEBUG_KALLOC
/// Returns the number of 4K allocations currently used.
size_t kalloc_debug_get_allocation_count();
#endif  // CONFIG_DEBUG_KALLOC

/// @brief Returns free memory in bytes
size_t kalloc_get_free_memory();

void kalloc_dump_free_memory();
