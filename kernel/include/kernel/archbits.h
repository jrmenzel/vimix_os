/* SPDX-License-Identifier: MIT */
#pragma once

#if (__riscv_xlen == 32)
#define _arch_is_32bit
#define _arch_bits_string "32"
#elif (__riscv_xlen == 64)
#define _arch_is_64bit
#define _arch_bits_string "64"
#endif

#ifndef _arch_bits_string
// lets assume the host is 64 bit
#define _arch_is_64bit
#define _arch_bits_string "64"
#endif
