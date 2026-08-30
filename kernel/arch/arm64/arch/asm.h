/* SPDX-License-Identifier: MIT */
#pragma once

#define ARCH_ASM_NOP asm volatile("nop")

#define ARCH_ASM_WAIT_CLOCKS(N)    \
    for (size_t i = 0; i < N; ++i) \
    {                              \
        ARCH_ASM_NOP;              \
    }
