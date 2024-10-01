/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

///
typedef size_t pte_t;

// flags per page
#define PTE_V (1L << 0)  ///< valid
#define PTE_R (1L << 1)  ///< readable
#define PTE_W (1L << 2)  ///< writeable
#define PTE_X (1L << 3)  ///< executable
#define PTE_U (1L << 4)  ///< user can access
#define PTE_G (1L << 5)  ///< global mapping (=exists in all address spaces)
#define PTE_A \
    (1L << 6)  ///< page has been accessed (r/w/x) since last time this bit was
               ///< cleared
#define PTE_D \
    (1L << 7)  ///< page has been written to since last time this bit
               ///< was cleared

#define PTE_RW (PTE_R | PTE_W)

/// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((size_t)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

/// how to map MMIO devices: R/W for the kernel
#define PTE_MMIO_FLAGS (PTE_RW)

/// how to map the kernel code: read only
#define PTE_RO_TEXT (PTE_R | PTE_X)

// how to map the kernel stack of the processes
#define PTE_KERNEL_STACK (PTE_R | PTE_W)

/// how to map kernel data and all RAM
#define PTE_RW_RAM (PTE_RW)

// how to map the user space init code: RWX
#define PTE_INITCODE (PTE_W | PTE_R | PTE_X | PTE_U)

/// how to map non-code for the user (heap & stack)
#define PTE_USER_RAM (PTE_R | PTE_W | PTE_U)

// is a non-leaf node valid?
#define PTE_IS_VALID_NODE(pte) (pte & PTE_V)

// One of RWX flags -> PTE is a leaf node
#define PTE_IS_LEAF(pte) (pte & (PTE_RW | PTE_X))

#define PTE_IS_WRITEABLE(p) (p & PTE_W)

/// to avoid remaps
#define PTE_IS_IN_USE(p) (p & PTE_V)

/// all mapped paged are ORed with this
#define PTE_MAP_DEFAULT_FLAGS (PTE_V | PTE_A | PTE_D)

#define PAGE_SHIFT 12  ///< bits of offset within a page

#if (__riscv_xlen == 32)
// 32 bit
#define PXMASK 0x3FF  // 10 bits
#define PXSHIFT(level) (PAGE_SHIFT + (10 * (level)))
#define PAGE_TABLE_MAX_LEVELS 2
/// Extract one of the two 10-bit page table index from a virtual address.
/// va is [[10 bit idx level 1][10 bit idx level 0][12 bits address in page]]
#define PAGE_TABLE_INDEX(level, va) \
    ((((size_t)(va)) >> PXSHIFT(level)) & PXMASK)

#else
// 64 bit

/// extract the three 9-bit page table indices from a virtual address.
#define PXMASK 0x1FF  // 9 bits
#define PXSHIFT(level) (PAGE_SHIFT + (9 * (level)))
#define PAGE_TABLE_MAX_LEVELS 3

/// Extract one of the three 9-bit page table indices (one per level) from a
/// virtual address. va is [[0][9 bit idx level 2][9 bit idx level 1][9 bit idx
/// level 0][12 bits address in page]]
#define PAGE_TABLE_INDEX(level, va) \
    ((((size_t)(va)) >> PXSHIFT(level)) & PXMASK)
#endif

/// Reconstruct a part of the virtual address, do this for all levels to get the
/// full virtual address
#define VA_FROM_PAGE_TABLE_INDEX(level, pti) (pti << PXSHIFT(level))
