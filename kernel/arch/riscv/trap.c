/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <arch/riscv/asm/registers.h>
#include <arch/riscv/scause.h>
#include <arch/timer.h>
#include <arch/trap.h>
#include <arch/trapframe.h>
#include <drivers/device.h>
#include <drivers/uart16550.h>
#include <drivers/virtio_disk.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/trap.h>
#include <mm/memlayout.h>
#include <mm/vm.h>
#include <syscalls/syscall.h>

extern char trampoline[], u_mode_trap_vector[], return_to_user_mode_asm[];

/// in s_mode_trap_vector.S, calls kernel_mode_interrupt_handler().
extern void s_mode_trap_vector();

extern size_t g_boot_hart;

void set_supervisor_trap_vector() { cpu_set_trap_vector(s_mode_trap_vector); }

void dump_pre_int_kthread_state(size_t *stack)
{
    // The kernel trap vector (s_mode_trap_vector) is using the same stack as
    // the previous kernel thread. The register state of that thread is stored
    // in the stack.
    if (stack == NULL) return;

    printk("stack: " FORMAT_REG_SIZE " | CPU ID (tp): %zd\n", (size_t)stack,
           stack[IDX_TP]);
    printk("ra  = " FORMAT_REG_SIZE "\n", stack[IDX_RA]);
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
}

void dump_exception_cause(struct Interrupt_Context *ctx)
{
    printk("scause (0x%zx): %s\n", ctx->scause,
           scause_exception_code_to_string(ctx->scause));
    printk("sepc: " FORMAT_REG_SIZE " stval: 0x%zx\n", rv_read_csr_sepc(),
           ctx->stval);

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

            pte_t *pte = vm_walk(proc->pagetable, ctx->stval, false);
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

void return_to_user_mode()
{
    // we're about to switch the destination of traps from
    // kernel_mode_interrupt_handler() to user_mode_interrupt_handler(), so turn
    // off interrupts until we're back in user space, where
    // user_mode_interrupt_handler() is correct.
    cpu_disable_interrupts();

    struct process *proc = get_current();

    // send syscalls, interrupts, and exceptions to u_mode_trap_vector in
    // u_mode_trap_vector.S
    size_t trampoline_u_mode_trap_vector =
        TRAMPOLINE + ((size_t)u_mode_trap_vector - (size_t)trampoline);

    cpu_set_trap_vector((void *)trampoline_u_mode_trap_vector);

    // set up trapframe values that u_mode_trap_vector will need when
    // the process next traps into the kernel.
    proc->trapframe->kernel_page_table =  // mmu_get_page_table_address();
        mmu_get_page_table_reg_value();   // kernel page table
    proc->trapframe->kernel_sp =
        proc->kstack + KERNEL_STACK_SIZE;  // process's kernel stack
    proc->trapframe->kernel_trap = (size_t)user_mode_interrupt_handler;
    proc->trapframe->kernel_hartid = smp_processor_id();

    // set up the registers that u_mode_trap_vector.S's sret will use
    // to get to user space.

    // set S Previous Privilege mode to User.
    xlen_t x = rv_read_csr_sstatus();
    x &= ~SSTATUS_SPP;  // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE;  // enable interrupts in user mode
    rv_write_csr_sstatus(x);

    // set S Exception Program Counter to the saved user pc.
    rv_write_csr_sepc(trapframe_get_program_counter(proc->trapframe));

    // tell u_mode_trap_vector.S the user page table to switch to.
    size_t satp = mmu_make_page_table_reg((size_t)proc->pagetable, 0);

    // jump to return_to_user_mode_asm in u_mode_trap_vector.S at the top of
    // memory, which switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    size_t return_to_user_mode_asm_ptr =
        TRAMPOLINE + ((size_t)return_to_user_mode_asm - (size_t)trampoline);
    ((void (*)(size_t, size_t))return_to_user_mode_asm_ptr)(satp, 0);
}

void handle_device_interrupt()
{
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();
    bool irq_handled = false;

    struct Device *dev = dev_by_irq_number(irq);
    if (dev)
    {
        dev->dev_ops.interrupt_handler(dev->device_number);
        irq_handled = true;
    }

    if (irq_handled == false && irq)
    {
        printk("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if (irq)
    {
        plic_complete(irq);
    }
}

void handle_timer_interrupt()
{
    uint64_t timer_interrupt_interval =
        g_timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;
    uint64_t now = rv_get_time();
    timer_schedule_interrupt(now + timer_interrupt_interval);

    // will only update on the CPU that booted first
    if (smp_processor_id() == g_boot_hart)
    {
        kticks_inc_ticks();
    }
}
