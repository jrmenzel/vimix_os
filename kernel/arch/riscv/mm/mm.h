/* SPDX-License-Identifier: MIT */
#pragma once
#include <kernel/page.h>

#if (__riscv_xlen == 64)
/// one beyond the highest possible virtual address.
/// USER_VA_END is actually one bit less than the max allowed by
/// Sv39, to avoid having to sign-extend virtual addresses
/// that have the high bit set.
#define USER_VA_END (1L << (9 + 9 + 9 + PAGE_SHIFT - 1))
#else
// by convention the user space becomes (only) the bottom half (2GB)
#define USER_VA_END 0x80000000
#endif
