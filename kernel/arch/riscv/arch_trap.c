/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/riscv/asm/registers.h>
#include <kernel/kticks.h>
#include <kernel/timer.h>
#include <kernel/trap.h>
#include <lib/panic.h>

/// in s_mode_trap_vector.S, calls kernel_mode_interrupt_handler().
extern void s_mode_trap_vector();

void set_supervisor_trap_vector()
{
    cpu_set_trap_vector((size_t)s_mode_trap_vector);
}

void dump_pre_int_kthread_state(size_t *stack)
{
    // The kernel trap vector (s_mode_trap_vector) is using the same stack as
    // the previous kernel thread. The register state of that thread is stored
    // in the stack.
    if (stack == NULL) return;

    printk("stack: " FORMAT_REG_SIZE " | CPU ID (tp): %zd\n", (size_t)stack,
           stack[IDX_TP]);
    debug_print_ra(stack[IDX_RA]);
    printk("sp  = " FORMAT_REG_SIZE "\n", (size_t)stack);
    printk("gp  = " FORMAT_REG_SIZE "\n", stack[IDX_GP]);
    printk("a0  = " FORMAT_REG_SIZE "\n", stack[IDX_A0]);
    printk("a1  = " FORMAT_REG_SIZE "\n", stack[IDX_A1]);
    printk("a2  = " FORMAT_REG_SIZE "\n", stack[IDX_A2]);
    printk("a3  = " FORMAT_REG_SIZE "\n", stack[IDX_A3]);
    printk("a4  = " FORMAT_REG_SIZE "\n", stack[IDX_A4]);
    printk("a5  = " FORMAT_REG_SIZE "\n", stack[IDX_A5]);
    printk("a6  = " FORMAT_REG_SIZE "\n", stack[IDX_A6]);
    printk("a7  = " FORMAT_REG_SIZE "\n", stack[IDX_A7]);
    printk("s0/fp = " FORMAT_REG_SIZE "\n", stack[IDX_S0]);
    printk("s1  = " FORMAT_REG_SIZE "\n", stack[IDX_S1]);
}

void dump_exception_cause(struct Interrupt_Context *ctx)
{
    printk("scause (0x%zx): %s\n", ctx->scause,
           scause_exception_code_to_string(ctx->scause));
    printk("stval: 0x%zx - sepc: " FORMAT_REG_SIZE " = ", ctx->stval,
           rv_read_csr_sepc());
    debug_print_pc(rv_read_csr_sepc());
    printk("\n");

    if (ctx->scause == SCAUSE_INSTRUCTION_PAGE_FAULT ||
        ctx->scause == SCAUSE_LOAD_PAGE_FAULT ||
        ctx->scause == SCAUSE_STORE_AMO_PAGE_FAULT)
    {
        printk("Tried to ");
        if (ctx->scause == SCAUSE_INSTRUCTION_PAGE_FAULT)
        {
            printk("execute from");
        }
        else if (ctx->scause == SCAUSE_LOAD_PAGE_FAULT)
        {
            printk("read from");
        }
        else if (ctx->scause == SCAUSE_STORE_AMO_PAGE_FAULT)
        {
            printk("write to");
        }
        // stval is set to the offending memory address
        printk(" address 0x%zx %s\n", ctx->stval,
               (ctx->stval ? "" : "(dereferenced NULL pointer)"));

        struct process *proc = get_current();
        if (proc)
        {
            if (!VA_IS_IN_RANGE(ctx->stval))
            {
                printk("Address 0x%zx out of range\n", ctx->stval);
                return;
            }

            spin_lock(&proc->pagetable->lock);
            pte_t *pte = vm_walk(proc->pagetable, ctx->stval, false);
            spin_unlock(&proc->pagetable->lock);
            if (!pte)
            {
                printk("Page of address 0x%zx is not mapped\n", ctx->stval);
            }
            else
            {
                printk("Page of address 0x%zx access: ", ctx->stval);
                debug_vm_print_pte_flags(*pte);
                printk("\n");
            }
        }
    }
}
