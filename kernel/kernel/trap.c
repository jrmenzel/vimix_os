/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <arch/trap.h>
#include <arch/trapframe.h>
#include <kernel/proc.h>
#include <kernel/trap.h>
#include <syscalls/syscall.h>

void dump_exception_cause_and_kill_proc(struct process *proc,
                                        struct Interrupt_Context *ctx)
{
    printk(
        "\nFatal: unexpected exception\n"
        "Killing process with pid=%d\n",
        proc->pid);
    dump_exception_cause(ctx);
    printk("Process: %s\n", proc->name);
    debug_print_process_registers(proc->trapframe);
    printk("Call stack:\n");
    debug_print_call_stack_user(proc);
    printk("\n");
    proc_set_killed(proc);
}

/// Handle an interrupt, exception, or system call from user space.
/// called from u_mode_trap_vector.S, first C function after storing the
/// CPU state / registers in assembly.
void user_mode_interrupt_handler(size_t *stack)
{
    // exception / interrupt cause
    struct Interrupt_Context ctx;
    int_ctx_create(&ctx);

    if (int_ctx_call_from_supervisor(&ctx))
    {
        panic("user_mode_interrupt_handler was *not* called from user mode");
    }

    // send interrupts and exceptions to kernel_mode_interrupt_handler(),
    // since we're now in the kernel.
    set_supervisor_trap_vector();

    // save user program counter.
    struct process *proc = get_current();
    trapframe_set_program_counter(proc->trapframe,
                                  int_ctx_get_exception_pc(&ctx));

    bool yield_process = false;

    if (int_ctx_is_system_call(&ctx))
    {
        // system call
        if (proc_is_killed(proc)) exit(-1);

        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        proc->trapframe->epc += 4;

        // an interrupt will change sepc, scause, and sstatus,
        // so enable only now that we're done with those registers.
        cpu_enable_interrupts();

        syscall(proc);
    }
    else if (int_ctx_source_is_timer(&ctx))
    {
        handle_timer_interrupt();
        yield_process = true;
    }
    else if (int_ctx_source_is_device(&ctx))
    {
        handle_device_interrupt();
    }
    else if (int_ctx_source_is_page_fault(&ctx))
    {
        size_t sp = proc->trapframe->sp;
        size_t fault_addr = int_ctx_get_addr(&ctx);

        // If the app tried to write between the stack pointer and its stack
        // -> stack overflow. Also test if the sp isn't more than one page away
        // from the current stack as we will provide only one additional page.
        if ((sp <= fault_addr && fault_addr < proc->stack_low) &&
            (sp >= (proc->stack_low - PAGE_SIZE)))
        {
            if (!proc_grow_stack(proc))
            {
                // growing the stack failed
                dump_exception_cause_and_kill_proc(proc, &ctx);
            }
        }
        else
        {
            // some other page fault
            dump_exception_cause_and_kill_proc(proc, &ctx);
        }
    }
    else
    {
        // some other scause
        dump_exception_cause_and_kill_proc(proc, &ctx);
    }

    if (proc_is_killed(proc))
    {
        exit(-1);
    }

    if (yield_process)
    {
        yield();
    }

    return_to_user_mode();
}

void kernel_mode_interrupt_handler(size_t *stack)
{
    struct Interrupt_Context ctx;
    int_ctx_create(&ctx);

    if (!int_ctx_call_from_supervisor(&ctx))
    {
        panic(
            "kernel_mode_interrupt_handler was *not* called from supervisor "
            "mode");
    }
    if (cpu_is_interrupts_enabled())
    {
        panic("kernel_mode_interrupt_handler: interrupts are still enabled");
    }

    bool yield_process = false;
    if (int_ctx_source_is_timer(&ctx))
    {
        handle_timer_interrupt();
        yield_process = true;
    }
    else if (int_ctx_source_is_device(&ctx))
    {
        handle_device_interrupt();
    }
    else
    {
        printk(
            "\nFatal: unhandled interrupt in "
            "kernel_mode_interrupt_handler()\n");
        dump_exception_cause(&ctx);
        dump_pre_int_kthread_state(stack);
        panic("kernel_mode_interrupt_handler");
    }

    if (yield_process)
    {
        // give up the CPU if a process is running
        struct process *proc = get_current();
        if (proc != NULL && proc->state == RUNNING)
        {
            yield();
        }
    }

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by s_mode_trap_vector.S's sepc
    // instruction.
    int_ctx_restore(&ctx);
}
