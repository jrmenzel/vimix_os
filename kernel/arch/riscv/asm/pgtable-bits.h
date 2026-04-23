/* SPDX-License-Identifier: MIT */
#pragma once

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
