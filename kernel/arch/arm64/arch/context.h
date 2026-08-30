/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/arm64.h>
#include <kernel/kernel.h>

/// Saved registers for kernel context switches.
struct context
{
    size_t sp;

    // callee-saved registers
    size_t x18;
    size_t x19;
    size_t x20;
    size_t x21;
    size_t x22;
    size_t x23;
    size_t x24;
    size_t x25;
    size_t x26;
    size_t x27;
    size_t x28;
    size_t x29;
    size_t x30;
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
    return ctx->x30;
}

static inline void context_set_return_register(struct context *ctx,
                                               size_t value)
{
    ctx->x30 = value;
}

static inline void context_set_stack_pointer(struct context *ctx, size_t value)
{
    ctx->sp = value;
}

static inline size_t context_get_frame_pointer(struct context *ctx)
{
    return ctx->x29;
}
