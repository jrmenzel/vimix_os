/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

/// @brief Returns the current stack pointer value.
static inline size_t asm_read_stack_pointer()
{
#if (__has_builtin(__builtin_stack_address))
    return (size_t)__builtin_stack_address();
#else
    size_t x;
#if defined(__riscv_xlen)
    asm volatile("mv %0, sp" : "=r"(x));
#else
    asm volatile("mov %0, sp" : "=r"(x));
#endif
    return x;
#endif
}
