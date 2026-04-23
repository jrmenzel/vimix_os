/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/page.h>

#if defined(__ARCH_32BIT)
// 32 bit
#define PT_IDX_MASK 0x3FF  // 10 bits
#define PT_IDX_BITS 10

#else
// 64 bit
#define PT_IDX_MASK 0x1FF  // 9 bits
#define PT_IDX_BITS 9

#endif  // __ARCH_32BIT

/// Extract one of the MAX_LEVELS 9-bit (or 10-bit on 32 bit systems) page table
/// indices (one per level) from a virtual address. va is:
/// [[some unused bits][MAX_LEVELS indices][12 bits address in page]]
#define PAGE_TABLE_INDEX(level, va) \
    ((((size_t)(va)) >> PXSHIFT(level)) & PT_IDX_MASK)

/// Reconstruct a part of the virtual address, do this for all levels to get the
/// full virtual address
#define VA_FROM_PAGE_TABLE_INDEX(level, pti) (pti << PXSHIFT(level))

/// A super page of 4 MB (32 bit systems) or 2 MB (64 bit) size:
#define MEGA_PAGE_SIZE (1UL << ((PT_IDX_BITS) + (PAGE_SHIFT)))

///< rounds up an address to the next mega page address
#define MEGA_PAGE_ROUND_UP(addr) \
    (((addr) + MEGA_PAGE_SIZE - 1) & ~(MEGA_PAGE_SIZE - 1))

///< rounds down an address to the next lower mega page address
#define MEGA_PAGE_ROUND_DOWN(addr) ((addr) & ~(MEGA_PAGE_SIZE - 1))

#if defined(__ARCH_64BIT)
/// A super page of 1 GB in size (64 bit only):
#define GIGA_PAGE_SIZE (1UL << ((PT_IDX_BITS) + (PAGE_SHIFT) + (PAGE_SHIFT)))
#endif  // __ARCH_64BIT

#define PTE_BUILD(physical_address, flags) (PA2PTE(physical_address) | flags)

#define virt_to_phys(va) ((va) - PAGE_OFFSET + PHYS_OFFSET)

#define phys_to_virt(pa) ((pa) - PHYS_OFFSET + PAGE_OFFSET)

#define MMIO_OFFSET 0

#define mmio_virt_to_phys(va) ((va) - MMIO_OFFSET)
#define mmio_phys_to_virt(pa) ((pa) + MMIO_OFFSET)
