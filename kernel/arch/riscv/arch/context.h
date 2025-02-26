/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

/// Saved registers for kernel context switches.
struct context
{
    xlen_t ra;
    xlen_t sp;

    // callee-saved registers
    xlen_t s0;
    xlen_t s1;
    xlen_t s2;
    xlen_t s3;
    xlen_t s4;
    xlen_t s5;
    xlen_t s6;
    xlen_t s7;
    xlen_t s8;
    xlen_t s9;
    xlen_t s10;
    xlen_t s11;
};

/// @brief Stores the current register values and restores the ones from
/// src_of_register. As the ra register holds the functions return address,
/// and the sp register holds its stack pointer, the function return in
/// context_switch will return to the thread of execution that was stored in
/// src_of_register. (implementation: context_switch.S)
/// @param dst_for_registers Where the current registers are stored to.
/// @param src_of_register Where the new register values are loaded from.
void context_switch(struct context *dst_for_registers,
                    struct context *src_of_register);

static inline size_t context_get_return_register(struct context *ctx)
{
    return ctx->ra;
}

static inline void context_set_return_register(struct context *ctx,
                                               size_t value)
{
    ctx->ra = value;
}

static inline void context_set_stack_pointer(struct context *ctx, size_t value)
{
    ctx->sp = value;
}

static inline size_t context_get_frame_pointer(struct context *ctx)
{
    return ctx->s0;
}
