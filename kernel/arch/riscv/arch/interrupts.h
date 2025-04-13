/* SPDX-License-Identifier: MIT */
#pragma once

#include "clint.h"
#include "plic.h"
#include "riscv.h"
#include "sbi.h"
#include "scause.h"

/// init once per CPU after one CPU called init_interrupt_controller()
static inline void init_interrupt_controller_per_hart() { plic_init_per_cpu(); }

/// @brief Enables an interrupt if priority > 0.
/// @param irq The IRQ to enable.
/// @param priority 0 disables the interrupt.
static inline void interrupt_controller_set_interrupt_priority(
    uint32_t irq, uint32_t priority)
{
    plic_set_interrupt_priority(irq, priority);
}

struct Interrupt_Context
{
    size_t sepc;
    size_t sstatus;
    size_t scause;
    size_t stval;
};

static inline void int_ctx_create(struct Interrupt_Context *ctx)
{
    ctx->sepc = rv_read_csr_sepc();
    ctx->sstatus = rv_read_csr_sstatus();
    ctx->scause = rv_read_csr_scause();
    ctx->stval = rv_read_csr_stval();
}

static inline void int_ctx_restore(struct Interrupt_Context *ctx)
{
    rv_write_csr_sepc(ctx->sepc);
    rv_write_csr_sstatus(ctx->sstatus);
}

static inline bool int_ctx_call_from_supervisor(struct Interrupt_Context *ctx)
{
    return (ctx->sstatus & SSTATUS_SPP);
}

static inline bool int_ctx_is_system_call(struct Interrupt_Context *ctx)
{
    return (ctx->scause == SCAUSE_ECALL_FROM_U_MODE);
}

static inline bool int_ctx_source_is_timer(struct Interrupt_Context *ctx)
{
    return ((ctx->scause == SCAUSE_SUPERVISOR_SOFTWARE_INTERRUPT ||
             ctx->scause == SCAUSE_SUPERVISOR_TIMER_INTERRUPT));
}

static inline bool int_ctx_source_is_device(struct Interrupt_Context *ctx)
{
    return (ctx->scause == SCAUSE_SUPERVISOR_EXTERNAL_INTERRUPT);
}

static inline bool int_ctx_source_is_page_fault(struct Interrupt_Context *ctx)
{
    return (ctx->scause == SCAUSE_STORE_AMO_PAGE_FAULT);
}

// e.g. where the page fault address is stored if the exception is a page fault
static inline size_t int_ctx_get_addr(struct Interrupt_Context *ctx)
{
    return ctx->stval;
}

static inline size_t int_ctx_get_exception_pc(struct Interrupt_Context *ctx)
{
    return ctx->sepc;
}
