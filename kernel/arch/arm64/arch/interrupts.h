/* SPDX-License-Identifier: MIT */

#pragma once

#include <arch/arm64/arm64.h>
#include <arch/arm64/asm/context_class_type.h>
#include <arch/arm64/drivers/gic_v2.h>
#include <arch/barrier.h>
#include <arch/irq.h>
#include <kernel/kernel.h>

struct Interrupt_Context
{
    size_t class;
    size_t type;
    size_t esr;
    size_t elr;
    size_t spsr;
    size_t far;
    int32_t pending_irq;
};

// ARM generic timer virtual timer interrupt is PPI 11 -> GIC ID 27.
#define ARM64_TIMER_IRQ_VIRTUAL 27
// Dedicated SGI used by VIMIX for outbound kernel IPIs.
#define ARM64_IPI_SGI_ID 15
// SGI range used for software interrupts / IPIs.
#define ARM64_SGI_ID_MAX 15

void int_ctx_create(struct Interrupt_Context *ctx, size_t ctx_class,
                    size_t ctx_type);

static inline void int_ctx_restore(struct Interrupt_Context *ctx)
{
    DEBUG_EXTRA_PANIC(ctx->elr != 0, "should not restore zero ELR");

    // ELR_EL1/SPSR_EL1 are single-copy architectural registers. Nested traps
    // can overwrite them, so restore the interrupted context before returning
    // through vector code that executes eret.
    arm_write_elr_el1(ctx->elr);
    arm_write_spsr_el1(ctx->spsr);
    isb();
}

static inline bool int_ctx_call_from_supervisor(struct Interrupt_Context *ctx)
{
    return (ctx->class == CTX_CLASS_CURRENT_EL_SP_ELX);
}

static inline bool int_ctx_is_system_call(struct Interrupt_Context *ctx)
{
    if (ctx->type != CTX_TYPE_SYNCHRONOUS)
    {
        return false;
    }

    return ESR_GET_EXC_CLASS(ctx->esr) == ESR_EC_SVC_A64;
}

bool int_ctx_source_is_timer(struct Interrupt_Context *ctx);

static inline bool int_ctx_source_is_device(struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    if (ctx->pending_irq == INVALID_IRQ_NUMBER)
    {
        return false;
    }

    // SGIs are IPIs; handle them in int_ctx_source_is_ipi().
    return ctx->pending_irq >= 16;
}

static inline bool int_ctx_source_is_page_fault(struct Interrupt_Context *ctx)
{
    if (ctx->type != CTX_TYPE_SYNCHRONOUS)
    {
        return false;
    }

    size_t ec = ESR_GET_EXC_CLASS(ctx->esr);
    return (ec == ESR_EC_INSN_ABORT_EL0) || (ec == ESR_EC_INSN_ABORT_EL1) ||
           (ec == ESR_EC_DATA_ABORT_EL0) || (ec == ESR_EC_DATA_ABORT_EL1);
}

// e.g. where the page fault address is stored if the exception is a page fault
static inline size_t int_ctx_get_addr(struct Interrupt_Context *ctx)
{
    return ctx->far;
}

static inline size_t int_ctx_get_exception_pc(struct Interrupt_Context *ctx)
{
    return ctx->elr;
}

static inline void acknowledge_gic2_claim()
{
    int32_t irq = gic2_claim();
    if (irq != INVALID_IRQ_NUMBER)
    {
        gic2_complete(irq);
    }
}

static inline void int_acknowledge_timer() { acknowledge_gic2_claim(); }

static inline void int_acknowledge_ipi() { acknowledge_gic2_claim(); }

bool int_ctx_source_is_ipi(struct Interrupt_Context *ctx);

static inline bool int_ctx_source_is_spurious(struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    return ctx->pending_irq == INVALID_IRQ_NUMBER;
}

static inline bool int_ctx_source_is_unclassified_async(
    struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    return !int_ctx_source_is_timer(ctx) && !int_ctx_source_is_ipi(ctx) &&
           !int_ctx_source_is_device(ctx) && !int_ctx_source_is_spurious(ctx);
}

static inline void int_consume_unclassified_async(struct Interrupt_Context *ctx)
{
    acknowledge_gic2_claim();
}

static inline int32_t int_ctx_get_pending_irq(struct Interrupt_Context *ctx)
{
    return ctx->pending_irq;
}

static inline size_t int_ctx_get_async_timer_state(
    struct Interrupt_Context *ctx)
{
    (void)ctx;
    return arm_read_cntv_ctl_el0();
}
