/* SPDX-License-Identifier: MIT */
#pragma once

#if (__riscv_xlen == 32)
#define __ARCH_is_32bit
#define __ARCH_bits_string "32"
#elif (__riscv_xlen == 64)
#define __ARCH_is_64bit
#define __ARCH_bits_string "64"
#endif

#ifndef __ARCH_bits_string
// lets assume the host is 64 bit
#define __ARCH_is_64bit
#define __ARCH_bits_string "64"
#endif
