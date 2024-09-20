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
typedef unsigned int  size_t;
typedef          int ssize_t;
#else
typedef unsigned long int  size_t;
typedef          long int ssize_t;
#endif


typedef size_t intptr_t;

// clang-format on
