/* SPDX-License-Identifier: MIT */
#pragma once

#ifndef __ASSEMBLY__
#include <kernel/types.h>
#endif

///< bits of offset within a page, defines PAGE_SIZE
///< Only 12 = 4K pages is supported
#define PAGE_SHIFT 12

///< bytes per page
#define PAGE_SIZE (1UL << PAGE_SHIFT)

///< rounds up an address to the next page address
#define PAGE_ROUND_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

///< rounds down an address to the next lower page address
#define PAGE_ROUND_DOWN(addr) (((addr)) & ~(PAGE_SIZE - 1))

#ifndef __ASSEMBLY__

/// @brief A single Page Table Entry. Common MMUs use full pages to store 1024
/// (32 bit) or 512 (64 bit) PTEs which either point to the next level of the
/// page table tree or define a leave node. The meaning of the individual bits
/// (e.g. access flags) depend on the architecture.
typedef size_t pte_t;

/// @brief Pointer to one page of PTEs - in the end a pagetable_t is a:
///  "size_t tagetable[512]" (64 bit)
///  "size_t tagetable[1024]" (32 bit)
typedef pte_t *pagetable_t;
static const pagetable_t INVALID_PAGETABLE_T = NULL;

#endif  // __ASSEMBLY__
