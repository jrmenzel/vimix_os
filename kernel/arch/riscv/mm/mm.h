/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/param.h>  // for PAGE_SIZE

#define PAGE_SHIFT 12  ///< bits of offset within a page

/// rounds up an address to the next page address
#define PAGE_ROUND_UP(sz) (((sz) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/// rounds down an address to the next lower page address
#define PAGE_ROUND_DOWN(a) (((a)) & ~(PAGE_SIZE - 1))

// flags per page
#define PTE_V (1L << 0)  ///< valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)  ///< user can access
#define PTE_G (1L << 5)
#define PTE_A (1L << 6)  ///< access bit
#define PTE_D (1L << 7)  ///< dirty bit

/// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((size_t)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

#if (__riscv_xlen == 32)
// 32 bit
#define PXMASK 0x3FF  // 10 bits
#define PXSHIFT(level) (PAGE_SHIFT + (10 * (level)))
/// Extract one of the two 10-bit page table index from a virtual address.
/// va is [[10 bit idx level 1][10 bit idx level 0][12 bits address in page]]
#define PAGE_TABLE_INDEX(level, va) \
    ((((size_t)(va)) >> PXSHIFT(level)) & PXMASK)

#else
// 64 bit

/// extract the three 9-bit page table indices from a virtual address.
#define PXMASK 0x1FF  // 9 bits
#define PXSHIFT(level) (PAGE_SHIFT + (9 * (level)))

/// Extract one of the three 9-bit page table indices (one per level) from a
/// virtual address. va is [[0][9 bit idx level 2][9 bit idx level 1][9 bit idx
/// level 0][12 bits address in page]]
#define PAGE_TABLE_INDEX(level, va) \
    ((((size_t)(va)) >> PXSHIFT(level)) & PXMASK)

/// one beyond the highest possible virtual address.
/// MAXVA is actually one bit less than the max allowed by
/// Sv39, to avoid having to sign-extend virtual addresses
/// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + PAGE_SHIFT - 1))
#endif
