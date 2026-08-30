/* SPDX-License-Identifier: MIT */
#pragma once

// Can be included from assembly, so use defines only!

#define EL_MASK 12
#define EL_0 0
#define EL_1 4
#define EL_2 8
#define EL_3 12

// SCTLR_EL1, System Control Register (EL1).
#define SCTLR_RESERVED \
    ((3 << 28) | (3 << 22) | (1 << 20) | (1 << 11) | (1 << 8) | (1 << 7))
#define SCTLR_EE_LITTLE_ENDIAN (0 << 25)
#define SCTLR_E0E_LITTLE_ENDIAN (0 << 24)
#define SCTLR_I_CACHE (1 << 12)
#define SCTLR_D_CACHE (1 << 2)
#define SCTLR_MMU_DISABLED (0 << 0)
#define SCTLR_MMU_ENABLED (1 << 0)

#define SCTLR_VALUE_MMU_DISABLED                                         \
    (SCTLR_RESERVED | SCTLR_EE_LITTLE_ENDIAN | SCTLR_E0E_LITTLE_ENDIAN | \
     SCTLR_I_CACHE | SCTLR_D_CACHE | SCTLR_MMU_DISABLED)

// HCR_EL2, Hypervisor Configuration Register (EL2).
#define HCR_RW (1 << 31)
#define HCR_VALUE HCR_RW

// CPACR_EL1, Architectural Feature Access Control Register.
#define CPACR_FP_EN (3 << 20)
#define CPACR_TRACE_EN (0 << 28)
#define CPACR_VALUE (CPACR_FP_EN | CPACR_TRACE_EN)

// SPSR_EL1/2/3, Saved Program Status Register.
#define SPSR_MASK_ALL (7 << 6)
#define SPSR_EL1h (5 << 0)
#define SPSR_EL2h (9 << 0)
#define SPSR_EL2_VALUE (SPSR_MASK_ALL | SPSR_EL1h)

// Exception Class in ESR_EL1.
#define EC_UNKNOWN 0x00

// Memory types

/// Index as used for mair_el1 and in the page tables.
/// The full memory attributes don't fit into the page table entries,
/// but an index (3 bits) into the MAIR_EL1 register does.
#define MT_DEVICE_nGnRnE_IDX 0
#define MT_NORMAL_CACHE_IDX 1
#define MT_NORMAL_NCACHE_IDX 2

/// device memory (no gathering, no reordering, no early write)
#define MT_DEVICE_nGnRnE_FLAGS 0x00
/// normal memory (Inner/Outer Write-back Non-transient RW-Allocate)
#define MT_NORMAL_FLAGS 0xFF
/// non cacheable memory (Inner/Outer Non-cacheable)
#define MT_NORMAL_NC_FLAGS 0x44

/// saves 8 memory types (size int8)
/// set once per code during boot and never changes.
#define MAIR_VALUE                                            \
    ((MT_DEVICE_nGnRnE_FLAGS << (8 * MT_DEVICE_nGnRnE_IDX)) | \
     (MT_NORMAL_FLAGS << (8 * MT_NORMAL_CACHE_IDX)) |         \
     (MT_NORMAL_NC_FLAGS << (8 * MT_NORMAL_NCACHE_IDX)))

//
// Translation Control Register: defines how the MMU works
//

// Size offset for TTBR0_EL1
#define TCR_T0SZ (64 - 48)

// cacheability of TTBR0_EL1 page tables pages: normal memory
#define TCR_ORGN0_IRGN0 ((1UL << 10) | (1UL << 8))

// shareability of TTBR0_EL1 page tables pages
#define TCR_SH0_NONE (0UL << 12)
#define TCR_SH0_OUTER (2UL << 12)
#define TCR_SH0_INNER (3UL << 12)

// page size of TTBR0_EL1 (values differ from TG1)
#define TCR_TG0_4K (0UL << 14)
#define TCR_TG0_16K (2UL << 14)
#define TCR_TG0_64K (1UL << 14)

// Size offset for TTBR1_EL1
#define TCR_T1SZ ((64 - 48) << 16)

// if set, TTBR0_EL1 defines ASID, if unset TTBR1_EL1
#define TCR_A1 (1UL << 22)

// Translation table walk disable for TTBR1_EL1
#define TCR_EPD1 (1UL << 23)

// cacheability of TTBR1_EL1 page tables pages: normal memory
#define TCR_ORGN1_IRGN1 ((1UL << 26) | (1UL << 24))

// shareability of TTBR1_EL1 page tables pages
#define TCR_SH1_NONE (0UL << 28)
#define TCR_SH1_OUTER (2UL << 28)
#define TCR_SH1_INNER (3UL << 28)

// page size of TTBR1_EL1 (values differ from TG0)
#define TCR_TG1_4K (2UL << 30)
#define TCR_TG1_16K (1UL << 30)
#define TCR_TG1_64K (3UL << 30)

// use 16 instead of 8 bit ASIDs
#define TCR_ASID_SIZE (1UL << 36)

#define TCR_VALUE                                                    \
    (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4K | TCR_TG1_4K | TCR_SH0_OUTER | \
     TCR_SH1_OUTER | TCR_ORGN0_IRGN0 | TCR_ORGN1_IRGN1 | TCR_ASID_SIZE)

#define TTBR_ASID_POS (48UL)
#define TTBR_ASID_MASK (0xFFFF000000000000UL)

// page table with this flag can be shared between cores.
#define TTBR_CNP (1UL)
