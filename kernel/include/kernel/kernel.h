/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/types.h>

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

// 32 or 64 depending on the architecture
#define BITS_PER_SIZET (8 * sizeof(size_t))

// clang-format off
#define infinite_loop \
    _Pragma("GCC diagnostic push"); \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-infinite-loop\""); \
    while (true) {} \
    _Pragma("GCC diagnostic pop");
// clang-format on
