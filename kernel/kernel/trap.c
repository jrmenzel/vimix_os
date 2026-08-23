/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <arch/irq.h>
#include <arch/trap.h>
#include <arch/trapframe.h>
#include <drivers/device.h>
#include <fs/sysfs/sys_kernel.h>
#include <init/start.h>
#include <kernel/interrupt_controller.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/trap.h>
#include <mm/arch_vm.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

void dump_exception_cause_and_kill_proc(struct process *proc,
                                        struct Interrupt_Context *ctx)
{
    uint32_t verbosity = sys_kernel_get_app_crash_verbosity();

    if (verbosity > 0)
    {
        printk(
            "\nFatal: unexpected exception\n"
            "Killing process %s, pid: %d\n",
            proc->name, proc->pid);
        dump_exception_cause(ctx, proc);

        if (verbosity > 1)
        {
            debug_print_process_registers(proc->trapframe);
            printk("Call stack:\n");
            debug_print_call_stack_user(proc);
            printk("\n");
            debug_print_memory_map(&proc->pagetable->memory_map);
        }
    }
    proc_set_killed(proc);
}

bool source_is_software_timer(struct Interrupt_Context *ctx)
{
#if defined(CONFIG_RISCV_BOOT_M_MODE)
    return m_mode_consume_timer_event();
#endif

    return false;
}

/// Handle an interrupt, exception, or system call from user space.
/// called from u_mode_trap_vector.S, first C function after storing the
/// CPU state / registers in assembly.
void user_mode_interrupt_handler(size_t *stack, size_t ctx_1, size_t ctx_2,
                                 size_t kernel_page_table_epoch)
{
    // save epoch
    if (kernel_page_table_epoch != 0)
    {
        g_cpus[smp_processor_id()].kernel_pgtable_epoch_seen =
            kernel_page_table_epoch;
    }

    // exception / interrupt cause
    struct Interrupt_Context ctx;
    int_ctx_create(&ctx, ctx_1, ctx_2);

    if (int_ctx_call_from_supervisor(&ctx))
    {
        printk(
            "User mode interrupt handler called from kernel mode. This should "
            "never "
            "happen.\n");
        dump_exception_cause_and_kill_proc(get_current(), &ctx);
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
        if (proc_is_killed(proc)) do_exit(-1);

        size_t return_addr = trapframe_get_program_counter(proc->trapframe);
        return_addr = cpu_get_next_inst_after_syscall(return_addr);
        trapframe_set_program_counter(proc->trapframe, return_addr);

        // an interrupt will change sepc, scause, and sstatus,
        // so enable only now that we're done with those registers.
        cpu_enable_interrupts();
        syscall(proc);
    }
    else if (int_ctx_source_is_timer(&ctx))
    {
        int_acknowledge_timer();
        handle_timer_interrupt();
        yield_process = true;
    }
    else if (int_ctx_source_is_ipi(&ctx))
    {
        int_acknowledge_ipi();
        yield_process = handle_ipi_interrupt();

        if (source_is_software_timer(&ctx))
        {
            handle_timer_interrupt();
            yield_process = true;
        }
    }
    else if (int_ctx_source_is_device(&ctx))
    {
        handle_device_interrupt();
    }
    else if (int_ctx_source_is_page_fault(&ctx))
    {
        size_t fault_addr = int_ctx_get_addr(&ctx);

        // If the app tried to write between a bit beyond its stack
        // -> stack overflow.
        if ((fault_addr < proc->stack_low) &&
            (fault_addr >= (proc->stack_low - PAGE_SIZE)))
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
        // some other cause
        dump_exception_cause_and_kill_proc(proc, &ctx);
    }

    if (proc_is_killed(proc))
    {
        do_exit(-1);
    }

    if (yield_process)
    {
        yield();
    }

    int_ctx_restore(&ctx);
    return_to_user_mode();
}

void return_to_user_mode_asm(size_t kernel_stack);

CAN_BE_CALLED_ON_USER_PAGE_TABLE void return_to_user_mode()
{
    // we're about to switch the destination of traps from
    // kernel_mode_interrupt_handler() to user_mode_interrupt_handler(), so turn
    // off interrupts until we're back in user space, where
    // user_mode_interrupt_handler() is correct.
    cpu_disable_interrupts();

    struct process *proc = get_current();
    size_t kernel_stack = proc->kstack + KERNEL_STACK_SIZE;

    // set up trapframe values that u_mode_trap_vector will need when
    // the process next traps into the kernel.
    proc->trapframe->kernel_page_table =
        mmu_get_kernel_pgtable_reg_value();     // kernel page table
    proc->trapframe->kernel_sp = kernel_stack;  // process's kernel stack
    proc->trapframe->kernel_trap = (size_t)user_mode_interrupt_handler;
    proc->trapframe->kernel_hartid = smp_processor_id();

    // set up the registers that u_mode_trap_vector.S's sret will use
    // to get to user space.

    // next environment return will switch to user mode.
    cpu_prepare_return_to_user_mode();

    cpu_set_user_stack_pointer(trapframe_get_stack_pointer(proc->trapframe));

    // set S Exception Program Counter to the saved user pc.
    cpu_set_exception_return_address(
        trapframe_get_program_counter(proc->trapframe));

    // switch to user page table, works as this function is mapped to trampsec
    mmu_set_user_page_table(proc->pagetable, 0);

    return_to_user_mode_asm(kernel_stack);
}

void kernel_mode_interrupt_handler(size_t *stack, size_t ctx_1, size_t ctx_2)
{
    struct Interrupt_Context ctx;
    int_ctx_create(&ctx, ctx_1, ctx_2);

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
        int_acknowledge_timer();
        handle_timer_interrupt();
        yield_process = true;
    }
    else if (int_ctx_source_is_ipi(&ctx))
    {
        int_acknowledge_ipi();
        yield_process = handle_ipi_interrupt();

        if (source_is_software_timer(&ctx))
        {
            handle_timer_interrupt();
            yield_process = true;
        }
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
        dump_exception_cause(&ctx, get_current());
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

void handle_device_interrupt()
{
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = g_int_con.claim();

    if (irq == INVALID_IRQ_NUMBER)
    {
        // no device claimed the interrupt
        return;
    }

    bool irq_handled = false;

    struct Device *dev = dev_by_irq_number(irq);
    if (dev)
    {
        DEBUG_EXTRA_PANIC((dev->dev_ops.interrupt_handler != NULL),
                          "Device has no interrupt handler\n");

        dev->dev_ops.interrupt_handler(dev->device_number);
        irq_handled = true;
    }

    if (irq_handled == false)
    {
        printk("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    g_int_con.complete(irq);
}

bool handle_ipi_interrupt()
{
    bool yield_process = false;

    spin_lock(&g_cpus_ipi_lock);
    struct cpu *c = get_cpu();
    for (size_t i = 0; i < MAX_IPI_PENDING; ++i)
    {
        enum ipi_type type = c->ipi[i].pending;
        // void *data = c->ipi[i].data;
        if (type == IPI_NONE) break;

        // clear the IPI
        c->ipi[i].pending = IPI_NONE;
        c->ipi[i].data = NULL;

        switch (type)
        {
            case IPI_KERNEL_PAGETABLE_CHANGED:
            {
                // a process changed the kernels page table, reload it to
                // flush TLBs
                spin_lock(&g_kernel_pagetable->lock);
                mmu_set_kernel_page_table(g_kernel_pagetable);
                spin_unlock(&g_kernel_pagetable->lock);
                break;
            }
            case IPI_KERNEL_PANIC:
            {
                // another CPU panicked, stop this CPUs scheduling
                struct cpu *this_cpu = get_cpu();
                this_cpu->state = CPU_PANICKED;
                yield_process = true;
                break;
            }
            case IPI_SHUTDOWN:
            {
                struct cpu *this_cpu = get_cpu();
                this_cpu->state = CPU_HALTED;
                yield_process = true;
                break;
            }

            default: printk("Unhandled IPI %d\n", type); break;
        }
    }
    spin_unlock(&g_cpus_ipi_lock);

    return yield_process;
}

void handle_timer_interrupt()
{
    uint64_t timer_interrupt_interval =
        g_timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;
    uint64_t now = get_time();
    timer_schedule_interrupt(now, timer_interrupt_interval);

    // Keep system time monotonic from the boot CPU
    if (smp_processor_id() == g_boot_hart)
    {
        kticks_inc_ticks();
    }
}
