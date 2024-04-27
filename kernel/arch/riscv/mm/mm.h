/* SPDX-License-Identifier: MIT */
#pragma once

#define PAGE_SIZE 4096  ///< bytes per page
#define PAGE_SHIFT 12   ///< bits of offset within a page

/// rounds up an address to the next page address
#define PAGE_ROUND_UP(sz) (((sz) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/// rounds down an address to the next lower page address
#define PAGE_ROUND_DOWN(a) (((a)) & ~(PAGE_SIZE - 1))

// flags per page
#define PTE_V (1L << 0)  // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)  // user can access

/// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((size_t)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte)&0x3FF)

/// extract the three 9-bit page table indices from a virtual address.
#define PXMASK 0x1FF  // 9 bits
#define PXSHIFT(level) (PAGE_SHIFT + (9 * (level)))

/// Extract one of the three 9-bit page table indices (one per level) from a
/// virtual address. va is [[0][9 bit idx level 2][9 bit idx level 1][9 bit idx
/// level 0][12 bits address in page]]
#define PX(level, va) ((((size_t)(va)) >> PXSHIFT(level)) & PXMASK)

/// one beyond the highest possible virtual address.
/// MAXVA is actually one bit less than the max allowed by
/// Sv39, to avoid having to sign-extend virtual addresses
/// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + PAGE_SHIFT - 1))
