/* SPDX-License-Identifier: MIT */

#include <arch/arm64/arm64.h>
#include <arch/arm64/asm/context_class_type.h>
#include <arch/arm64/drivers/gic_v2.h>
#include <arch/interrupts.h>
#include <arch/trapframe.h>
#include <init/start.h>
#include <kernel/kticks.h>
#include <kernel/printk.h>
#include <kernel/process.h>
#include <kernel/smp.h>
#include <kernel/timer.h>
#include <kernel/trap.h>
#include <lib/panic.h>
#include <lib/xdbg/xdbg.h>

extern void trap_vectors();

void set_supervisor_trap_vector() { cpu_set_trap_vector((size_t)trap_vectors); }

void dump_pre_int_kthread_state(size_t *stack)
{
    // The kernel trap vector (trap_vector.S) is using the same stack as
    // the previous kernel thread. The register state of that thread is
    // stored in the stack.
    if (stack == NULL) return;

    printk("stack: " FORMAT_REG_SIZE " | CPU ID: %zd\n", (size_t)stack,
           smp_processor_id());
    for (size_t i = 0; i < 30; ++i)
    {
        size_t stack_pos = 29 - i;
        if (i < 10) printk(" ");
        printk("x%zd = " FORMAT_REG_SIZE " ", i, stack[stack_pos]);
        if (i % 4 == 3) printk("\n");
    }
    printk("x30 = " FORMAT_REG_SIZE "\n", stack[30]);
    printk("\n");

    // stack[29] is the interrupted frame pointer (x29).
    size_t interrupted_fp = stack[29];
    if (interrupted_fp >= PAGE_OFFSET)
    {
        debug_print_call_stack_kernel_fp(interrupted_fp);
    }
    else
    {
        printk(
            "call stack unavailable: interrupted frame pointer is not a kernel "
            "address\n");
    }
}

void decode_data_abort(struct Interrupt_Context *ctx)
{
    uint32_t dfsc = (uint32_t)ESR_GET_DFSC(ctx->esr);
    printk(", ");
    if (dfsc <= 0x0f)
    {
        switch ((dfsc >> 2) & 0x3)
        {
            case 0: printk("Address size fault"); break;
            case 1: printk("Translation fault"); break;
            case 2: printk("Access flag fault"); break;
            case 3: printk("Permission fault"); break;
        }
        switch (dfsc & 0x3)
        {
            case 0: printk(" at level 0"); break;
            case 1: printk(" at level 1"); break;
            case 2: printk(" at level 2"); break;
            case 3: printk(" at level 3"); break;
        }
    }
    else if (dfsc == 0x10)
    {
        printk("Synchronous external abort");
    }
    else if ((dfsc >= 0x14) && (dfsc <= 0x17))
    {
        printk("Synchronous external abort on translation table walk");
        printk(" at level %u", (unsigned)(dfsc & 0x3));
    }
    else
    {
        printk("Data abort DFSC=0x%x", dfsc);
    }
    if (ctx->esr & ESR_DATA_FAULT_WNR)
    {
        printk(" write access");
    }
    else
    {
        printk(" read access");
    }
    if (ctx->esr & ESR_DATA_FAULT_S1PTW)
    {
        printk(" fault during page table walk!");
    }
}

void decode_instruction_abort(struct Interrupt_Context *ctx)
{
    uint32_t ifsc = (uint32_t)ESR_GET_IFSC(ctx->esr);
    printk(", ");
    if (ifsc <= 0x0F)
    {
        switch ((ifsc >> 2) & 0x3)
        {
            case 0: printk("Address size fault"); break;
            case 1: printk("Translation fault"); break;
            case 2: printk("Access flag fault"); break;
            case 3: printk("Permission fault"); break;
        }
        switch (ifsc & 0x3)
        {
            case 0: printk(" at level 0"); break;
            case 1: printk(" at level 1"); break;
            case 2: printk(" at level 2"); break;
            case 3: printk(" at level 3"); break;
        }
        if (ifsc == 0)
        {
            // print special meaning after "Address size fault at level 0"
            printk(" of translation or translation table base register");
        }
    }
    else
    {
        if (ifsc == ESR_IFSC_SYNC_EXT_ABORT)
        {
            printk(
                "abort not on translation table walk or hardware update of "
                "translation table");
            if (ctx->esr & ESR_INST_FAULT_FNV)
            {
                printk(" FAR not valid");
            }
        }
        if ((ifsc >> 2) == 0b0101)
        {
            int32_t level = ifsc & 0x3;
            printk(" abort on translation table walk at level %d", level);
        }
    }
    if (ctx->esr & ESR_INST_FAULT_S1PTW)
    {
        printk(" fault during page table walk!");
    }
}

void dump_exception_cause(struct Interrupt_Context *ctx, struct process *proc)
{
    size_t ec = ESR_GET_EXC_CLASS(ctx->esr);

    printk("Exception class: ");
    switch (ctx->class)
    {
        case CTX_CLASS_CURRENT_EL_SP_EL0: printk("Current EL, SP_EL0\n"); break;
        case CTX_CLASS_CURRENT_EL_SP_ELX: printk("Current EL, SP_EL1\n"); break;
        case CTX_CLASS_LOWER_EL_AARCH64:
            printk("Lower EL (EL0), AArch64\n");
            break;
        case CTX_CLASS_LOWER_EL_AARCH32: printk("Lower EL, AArch32\n"); break;
    }

    printk("Type: ");
    switch (ctx->type)
    {
        case CTX_TYPE_SYNCHRONOUS: printk("Synchronous"); break;
        case CTX_TYPE_IRQ: printk("IRQ"); break;
        case CTX_TYPE_FIQ: printk("Fast IRQ"); break;
        case CTX_TYPE_SERROR: printk("System Error"); break;
    }
    printk(": ");

    if (ctx->type != CTX_TYPE_SYNCHRONOUS)
    {
        // ESR_EL1 decode is only meaningful for synchronous exceptions.
        printk("Asynchronous interrupt");
    }
    else
    {
        switch (ec)
        {
            case ESR_EC_UNKNOWN:
                printk("Unknown exception cause in ESR");
                break;
            case ESR_EC_WFI_WFE: printk("Trapped WFI/WFE"); break;
            case ESR_EC_ILLEGAL_STATE: printk("Illegal execution"); break;
            case ESR_EC_SVC_A64: printk("System call"); break;
            case ESR_EC_INSN_ABORT_EL0:
                printk("Instruction abort, lower EL");
                decode_instruction_abort(ctx);
                break;
            case ESR_EC_INSN_ABORT_EL1:
                printk("Instruction abort, same EL");
                decode_instruction_abort(ctx);
                break;
            case ESR_EC_PC_ALIGNMENT_FAULT:
                printk("Instruction alignment fault");
                break;
            case ESR_EC_DATA_ABORT_EL0:
                printk("Data abort, lower EL");
                decode_data_abort(ctx);
                break;
            case ESR_EC_DATA_ABORT_EL1:
                printk("Data abort, same EL");
                decode_data_abort(ctx);
                break;
            case ESR_EC_SP_ALIGNMENT_FAULT:
                printk("Stack alignment fault");
                break;
            case ESR_EC_FP_TRAP_A64: printk("Floating point"); break;
            default: printk("Unknown: %zd", ec); break;
        }
    }

    // dump registers
    printk(":\n");
    printk("  ESR_EL1  0x%016lx (Exception Syndrome Register)\n", ctx->esr);
    printk("  ELR_EL1  0x%016lx (return address) ", ctx->elr);

    debug_print_pc(ctx->elr, proc ? proc->xdbg_info : NULL);
    printk("\n");

    printk("  SPSR_EL1 0x%016lx (Saved Program Status Register)\n", ctx->spsr);
    printk("  FAR_EL1  0x%016lx (Fault Address Register)\n", ctx->far);
    if (ctx->type != CTX_TYPE_SYNCHRONOUS)
    {
        size_t cntv_ctl = 0;
        arm_write_cntv_ctl_el0(cntv_ctl);
        printk("  PENDING_IRQ %d (GIC HPPIR snapshot)\n", ctx->pending_irq);
        printk("  CNTV_CTL_EL0 0x%016lx\n", cntv_ctl);
    }
    printk("\n");
}
