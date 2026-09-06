/* SPDX-License-Identifier: MIT */

#include <arch/arm64/arm64.h>
#include <arch/interrupts.h>
#include <kernel/kernel.h>

void int_ctx_create(struct Interrupt_Context *ctx, size_t ctx_class,
                    size_t ctx_type)
{
    ctx->esr = arm_read_esr_el1();
    ctx->elr = arm_read_elr_el1();
    ctx->spsr = arm_read_spsr_el1();
    ctx->far = arm_read_far_el1();
    ctx->class = ctx_class;
    ctx->type = ctx_type;
    ctx->pending_irq = INVALID_IRQ_NUMBER;
    ctx->ack_token = 0;
    // Synchronous exceptions must not acknowledge an unrelated interrupt.
    if ((ctx_type == CTX_TYPE_IRQ) || (ctx_type == CTX_TYPE_FIQ))
    {
        ctx->ack_token = gic2_acknowledge();
        uint32_t irq = ctx->ack_token & 0x3ffu;
        if (irq < 1020)
        {
            ctx->pending_irq = irq;
        }
    }
}

bool int_ctx_source_is_timer(struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    return ctx->pending_irq == ARM64_TIMER_IRQ_VIRTUAL;
}

bool int_ctx_source_is_ipi(struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    return (ctx->pending_irq >= 0) && (ctx->pending_irq <= ARM64_SGI_ID_MAX);
}
