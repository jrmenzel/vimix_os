/* SPDX-License-Identifier: MIT */

#include <arch/arm64/arm64.h>
#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

bool arm64_arch_timer_pending();
bool arm64_cpu_has_queued_ipi();

bool arm64_arch_timer_pending()
{
    size_t cntv_ctl = arm_read_cntv_ctl_el0();

    if (cntv_ctl & CNTV_CTL_IMASK)
    {
        return false;
    }

    return (cntv_ctl & CNTV_CTL_ISTATUS) != 0;
}

bool arm64_cpu_has_queued_ipi()
{
    bool has_queued_ipi = false;
    spin_lock(&g_cpus_ipi_lock);
    struct cpu *c = get_cpu();
    for (size_t i = 0; i < MAX_IPI_PENDING; ++i)
    {
        if (c->ipi[i].pending != IPI_NONE)
        {
            has_queued_ipi = true;
            break;
        }
    }
    spin_unlock(&g_cpus_ipi_lock);

    return has_queued_ipi;
}

void int_ctx_create(struct Interrupt_Context *ctx, size_t ctx_class,
                    size_t ctx_type)
{
    ctx->esr = arm_read_esr_el1();
    ctx->elr = arm_read_elr_el1();
    ctx->spsr = arm_read_spsr_el1();
    ctx->far = arm_read_far_el1();
    ctx->class = ctx_class;
    ctx->type = ctx_type;
    ctx->pending_irq = gic2_peek_pending();
}

bool int_ctx_source_is_timer(struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    if (ctx->pending_irq != INVALID_IRQ_NUMBER)
    {
        // A concrete pending GIC interrupt takes precedence.
        return ctx->pending_irq == ARM64_TIMER_IRQ_VIRTUAL;
    }

    // If software has queued an IPI for this CPU, do not classify this trap as
    // a timer fallback even if CNTV reports pending.
    if (arm64_cpu_has_queued_ipi())
    {
        return false;
    }

    // No pending GIC IRQ: fall back to architected timer state.
    return arm64_arch_timer_pending();
}

bool int_ctx_source_is_ipi(struct Interrupt_Context *ctx)
{
    if ((ctx->type != CTX_TYPE_IRQ) && (ctx->type != CTX_TYPE_FIQ))
    {
        return false;
    }

    if ((ctx->pending_irq >= 0) && (ctx->pending_irq <= ARM64_SGI_ID_MAX))
    {
        return true;
    }

    // On GICv2, HPPIR can report no pending IRQ while the taken SGI is
    // already active. Fall back to the software IPI queue state.
    return arm64_cpu_has_queued_ipi();
}
