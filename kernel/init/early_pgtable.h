/* SPDX-License-Identifier: MIT */
#pragma once

#include <asm/pgtable-bits.h>
#include <kernel/page.h>

// One page per level to map the kernel (assuming it fits, but should as long as
// it stays below 2MB) Plus one branch each to map:
// - the device tree
// - the first page of the kernel text with PA==VA
// - early RAM
// - some extra MB to map if there is a embedded ramdisk
//  (-1 for the branch as the root is always shared)
#if defined(__CONFIG_RAMDISK_EMBEDDED)
#define EARLY_PT_EXTRA_PAGES (8 * (PAGE_TABLE_MAX_LEVELS - 1))
#else
#define EARLY_PT_EXTRA_PAGES 0
#endif
#define EARLY_PT_RESERVED_PAGES                                \
    (PAGE_TABLE_MAX_LEVELS + 3 * (PAGE_TABLE_MAX_LEVELS - 1) + \
     EARLY_PT_EXTRA_PAGES)

#define EARLY_PAGE_TABLE_SIZE (PAGE_SIZE * EARLY_PT_RESERVED_PAGES)

// With 4KB pages the last level of the page table contains 1024 (32-bit)
// or 512 (64-bit) entries pointing to a total of 4MB / 2MB.
// Assuming the kernel including all static variables is smaller than 2MB
// and it's relocated to a 4MB / 2MB boundry, one leaf of the page table will
// contain the full mapping (ensuring only PAGE_TABLE_MAX_LEVELS pages are
// needed).
#ifndef __ASSEMBLY__

#include <init/start.h>
#include <kernel/pgtable.h>

extern char g_early_kernel_page_table[EARLY_PAGE_TABLE_SIZE];

size_t early_pgtable_init(size_t pt_paddr, size_t dtb_paddr,
                          size_t memory_map_paddr);

static inline size_t early_ram_begin()
{
    return PAGE_ROUND_UP((size_t)__end_of_kernel);
}

static inline size_t early_ram_end()
{
    size_t kernel_mega_page_end = MEGA_PAGE_ROUND_UP((size_t)__end_of_kernel);
    return kernel_mega_page_end + MEGA_PAGE_SIZE;
}

#endif
