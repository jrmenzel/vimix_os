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


#if defined(__ARCH_is_32bit)
typedef unsigned int  size_t;
typedef          int ssize_t;
#else
typedef unsigned long int  size_t;
typedef          long int ssize_t;
#endif
// clang-format on

typedef ssize_t intptr_t;
typedef size_t uintptr_t;

// Minimum of signed integral types
#define INT8_MIN (-128)
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483648)
#define INT64_MIN (-92233720368547758078)

// Maximum of signed integral types
#define INT8_MAX (127)
#define INT16_MAX (32767)
#define INT32_MAX (2147483647L)
#define INT64_MAX (9223372036854775807LL)

#define INT_MAX INT32_MAX

// Maximum of unsigned integral types
#define UINT8_MAX (255)
#define UINT16_MAX (65535)
#define UINT32_MAX (4294967295UL)
#define UINT64_MAX (18446744073709551615ULL)
