/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/vm.h>

/// @brief Allocate one 4096-byte page of physical memory.
/// Returns a pointer that the kernel can use.
/// @return One 4k page or NULL if the memory cannot be allocated.
void* kalloc();

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