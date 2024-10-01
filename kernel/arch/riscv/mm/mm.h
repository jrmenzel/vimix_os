/* SPDX-License-Identifier: MIT */
#pragma once

#define PAGE_SHIFT 12  ///< bits of offset within a page

#if (__riscv_xlen == 64)
/// one beyond the highest possible virtual address.
/// MAXVA is actually one bit less than the max allowed by
/// Sv39, to avoid having to sign-extend virtual addresses
/// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + PAGE_SHIFT - 1))
#endif
