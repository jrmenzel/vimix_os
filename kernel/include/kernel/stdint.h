/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/archbits.h>

// clang-format off
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef          char       int8_t;
typedef          short      int16_t;
typedef          int        int32_t;
typedef          long long  int64_t;

#if defined(_arch_is_32bit)
typedef uint32_t size_t;
typedef int32_t ssize_t;
#else
typedef uint64_t size_t;
typedef int64_t ssize_t;
#endif

typedef size_t intptr_t;

// clang-format on
