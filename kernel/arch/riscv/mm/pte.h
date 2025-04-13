/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/page.h>

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

static inline pte_t pte_set_executable(pte_t pte) { return pte | PTE_X; };
static inline pte_t pte_unset_executable(pte_t pte) { return pte & ~PTE_X; }
#define PTE_IS_EXECUTABLE(pte) (pte & PTE_X)

static inline pte_t pte_set_writeable(pte_t pte) { return pte | PTE_W; }
static inline pte_t pte_unset_writeable(pte_t pte) { return pte & ~PTE_W; }
#define PTE_IS_WRITEABLE(pte) (pte & PTE_W)

#define PTE_IS_READABLE(pte) (pte & PTE_R)

static inline pte_t pte_clear_user_access(pte_t pte) { return pte & ~PTE_U; }

#define PTE_IS_DIRTY(pte) (pte & PTE_D)
#define PTE_WAS_ACCESSED(pte) (pte & PTE_A)
#define PTE_IS_GLOBAL(pte) (pte & PTE_G)

#define PTE_RW (PTE_R | PTE_W)

/// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((size_t)pa) >> 12) << 10)

#define PTE_GET_PA(pte) (((pte) >> 10) << 12)

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

#define PTE_IS_USER_ACCESSIBLE(pte) (pte & PTE_U)

// is this a valid user accessible leaf?
#define PTE_IS_VALID_USER(pte) \
    (PTE_IS_VALID_NODE(pte) && PTE_IS_USER_ACCESSIBLE(pte))

// One of RWX flags -> PTE is a leaf node
#define PTE_IS_LEAF(pte) (pte & (PTE_RW | PTE_X))

#define PTE_MAKE_VALID_LEAF(pte) (pte = pte | PTE_V);

#define PTE_MAKE_VALID_TABLE(pte) (pte = (pte | PTE_V));

/// all mapped paged are ORed with this
#define PTE_MAP_DEFAULT_FLAGS (PTE_V | PTE_A | PTE_D)

#if (__riscv_xlen == 32)
// 32 bit

/// extract the two 10-bit page table indices from a virtual address.
#define PXSHIFT(level) (PAGE_SHIFT + (10 * (level)))
#define PAGE_TABLE_MAX_LEVELS 2

#else
// 64 bit

/// extract the three 9-bit page table indices from a virtual address.
#define PXSHIFT(level) (PAGE_SHIFT + (9 * (level)))
#define PAGE_TABLE_MAX_LEVELS 3

#endif
