/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

/// @brief Returns the current stack pointer value.
static inline size_t asm_read_stack_pointer()
{
// needs GCC, on RISCV it replaces:
// size_t x; asm volatile("mv %0, sp" : "=r"(x)); return x;
#if (__has_builtin(__builtin_stack_address))
    return (size_t)__builtin_stack_address();
#else
    size_t x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
#endif
}
