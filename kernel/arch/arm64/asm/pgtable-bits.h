/* SPDX-License-Identifier: MIT */
#pragma once

// ARM64 PTE flags:
#define PTE_V (1UL << 0)     ///< valid PTE
#define PTE_TYPE (1UL << 1)  ///< PTE type; 0 = table / 1 = page
#define PTE_U \
    (1UL << 6)  ///< access for: 0 = EL1 (kernel) only, 1 = also EL0 (user code)
#define PTE_RO (1UL << 7)  ///< page is read-only (RW if 0)
#define PTE_RW (0)
#define PTE_AF (1UL << 10)          ///< Access flag: 0 = unused, 1 = accessed
#define PTE_nG (1UL << 11)          ///< non global mapping
#define PTE_DBM (1UL << 51)         ///< Dirty bit modifier
#define PTE_PXN (1UL << 53)         ///< Privileged eXecute Never
#define PTE_UXN (1UL << 54)         ///< Unprivileged(user) eXecute Never
#define PTE_XN (PTE_PXN | PTE_UXN)  ///< eXecute Never

#define PTE_USER (PTE_U | PTE_nG)

// Shareable attribute
#define PTE_SH(sh) (((sh) & 3UL) << 8)
#define PTE_SH_OUTER PTE_SH(2)  // outer sharable
#define PTE_SH_INNER PTE_SH(3)  // inner sharable

#define PTE_MEM_TYPE_SHIFT(i) (((i) & 7UL) << 2)
#define PTE_DEVICE (PTE_MEM_TYPE_SHIFT(MT_DEVICE_nGnRnE_IDX) | PTE_SH_INNER)
#define PTE_NORMAL_NC (PTE_MEM_TYPE_SHIFT(MT_NORMAL_NCACHE_IDX) | PTE_SH_INNER)
#define PTE_NORMAL (PTE_MEM_TYPE_SHIFT(MT_NORMAL_CACHE_IDX) | PTE_SH_INNER)

/// how to map MMIO devices:
/// ARM needs a special device mapping to prevent speculative access (which
/// might cause issues with devices) It should also be not executable to prevent
/// speculative code reads.
/// https://developer.arm.com/documentation/102376/0200/Device-memory
#define PTE_MMIO_FLAGS (PTE_DEVICE | PTE_XN)

/// all mapped paged are ORed with this
#define PTE_MAP_DEFAULT_FLAGS (PTE_V | PTE_TYPE | PTE_AF)

#define PXSHIFT(level) (PAGE_SHIFT + (9 * (level)))
#define PAGE_TABLE_MAX_LEVELS 4

/// how to map all RAM
#define PTE_RW_RAM (PTE_NORMAL | PTE_XN)

#define PTE_KERNEL_RO_TEXT (PTE_NORMAL | PTE_RO)
// #define PTE_KERNEL_RW_TEXT
#define PTE_KERNEL_RO_DATA (PTE_NORMAL | PTE_RO | PTE_XN)
#define PTE_KERNEL_RW_DATA (PTE_NORMAL | PTE_RW_RAM)

// how to map the kernel stack of the processes
#define PTE_KERNEL_STACK (PTE_NORMAL | PTE_XN | PTE_nG)

#define PTE_USER_RO_TEXT (PTE_USER | PTE_NORMAL | PTE_RO)
#define PTE_USER_RW_TEXT (PTE_USER | PTE_NORMAL | PTE_RW)
#define PTE_USER_RO_DATA (PTE_USER | PTE_NORMAL | PTE_RO | PTE_XN)
#define PTE_USER_RW_DATA (PTE_USER | PTE_NORMAL | PTE_RW | PTE_XN)

#define PTE_TRAPFRAME (PTE_RW_RAM | PTE_nG)
