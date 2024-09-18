/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <arch/riscv/asm/registers.h>
#include <arch/riscv/sbi.h>
#include <arch/riscv/scause.h>
#include <arch/trap.h>
#include <drivers/device.h>
#include <drivers/uart16550.h>
#include <drivers/virtio_disk.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

// each call to the timer interrupt is one tick
struct spinlock g_tickslock;
size_t g_ticks;

extern char trampoline[], u_mode_trap_vector[], return_to_user_mode_asm[];

/// in s_mode_trap_vector.S, calls kernel_mode_interrupt_handler().
extern void s_mode_trap_vector();

void trap_init()
{
    g_ticks = 0;
    spin_lock_init(&g_tickslock, "time");
}

void set_s_mode_trap_vector()
{
    cpu_set_s_mode_trap_vector(s_mode_trap_vector);
}

void dump_process_info(struct process *proc)
{
    struct trapframe *tf = proc->trapframe;
    printk("Process: %s\n", proc->name);
    // clang-format off
    printk("ra:  0x%x; s0: 0x%x; a0: 0x%x; t0: 0x%x\n", tf->ra,  tf->s0, tf->a0, tf->t0);
    printk("sp:  0x%x; s1: 0x%x; a1: 0x%x; t1: 0x%x\n", tf->sp,  tf->s1, tf->a1, tf->t1);
    printk("gp:  0x%x; s2: 0x%x; a2: 0x%x; t2: 0x%x\n", tf->gp,  tf->s2, tf->a2, tf->t2);
    printk("tp:  0x%x; s3: 0x%x; a3: 0x%x; t3: 0x%x\n", tf->tp,  tf->s3, tf->a3, tf->t3);
    printk("s8:  0x%x; s4: 0x%x; a4: 0x%x; t4: 0x%x\n", tf->s8,  tf->s4, tf->a4, tf->t4);
    printk("s9:  0x%x; s5: 0x%x; a5: 0x%x; t5: 0x%x\n", tf->s9,  tf->s5, tf->a5, tf->t5);
    printk("s10: 0x%x; s6: 0x%x; a6: 0x%x; t6: 0x%x\n", tf->s10, tf->s6, tf->a6, tf->t6);
    printk("s11: 0x%x; s7: 0x%x; a7: 0x%x\n",           tf->s11, tf->s7, tf->a7);
    // clang-format on

    // debug_vm_print_page_table(proc->pagetable);
}

/// @brief Dump kernel thread state from before the interrupt.
void dump_pre_int_kthread_state(size_t *stack)
{
    // The kernel trap vector (s_mode_trap_vector) is using the same stack as
    // the previous kernel thread. The register state of that thread is stored
    // in the stack.
    if (stack == NULL) return;

    printk("stack: 0x%x | CPU ID (tp): %d\n", (size_t)stack, stack[IDX_TP]);
    printk("ra  = 0x%x\n", stack[IDX_RA]);
    printk("sp  = 0x%x\n", stack[IDX_SP]);
    printk("gp  = 0x%x\n", stack[IDX_GP]);

    printk("a0  = 0x%x\n", stack[IDX_A0]);
    printk("a1  = 0x%x\n", stack[IDX_A1]);
    printk("a2  = 0x%x\n", stack[IDX_A2]);
    printk("a3  = 0x%x\n", stack[IDX_A3]);
    printk("a4  = 0x%x\n", stack[IDX_A4]);
    printk("a5  = 0x%x\n", stack[IDX_A5]);
    printk("a6  = 0x%x\n", stack[IDX_A6]);
    printk("a7  = 0x%x\n", stack[IDX_A7]);
}

void handle_timer_interrupt();
void handle_plic_device_interrupt();

/// Handle an interrupt, exception, or system call from user space.
/// called from u_mode_trap_vector.S, first C function after storing the
/// CPU state / registers in assembly.
void user_mode_interrupt_handler()
{
    if ((rv_read_csr_sstatus() & SSTATUS_SPP) != 0)
    {
        panic("user_mode_interrupt_handler was *not* called from user mode");
    }

    // send interrupts and exceptions to kernel_mode_interrupt_handler(),
    // since we're now in the kernel.
    set_s_mode_trap_vector();

    // save user program counter.
    struct process *proc = get_current();
    trapframe_set_program_counter(proc->trapframe, rv_read_csr_sepc());

    // exception / interrupt cause
    xlen_t scause = rv_read_csr_scause();
    bool yield_process = false;

    if (scause == SCAUSE_ECALL_FROM_U_MODE)
    {
        // system call
        if (proc_is_killed(proc)) exit(-1);

        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        proc->trapframe->epc += 4;

        // an interrupt will change sepc, scause, and sstatus,
        // so enable only now that we're done with those registers.
        cpu_enable_device_interrupts();

        syscall(proc);
    }
    else if (scause == SCAUSE_SUPERVISOR_SOFTWARE_INTERRUPT ||
             scause == SCAUSE_SUPERVISOR_TIMER_INTERRUPT)
    {
        handle_timer_interrupt();
        yield_process = true;
    }
    else if (scause == SCAUSE_SUPERVISOR_EXTERNAL_INTERRUPT)
    {
        handle_plic_device_interrupt();
    }
    else
    {
        printk(
            "\nFatal: unexpected scause/exception in "
            "user_mode_interrupt_handler()\n"
            "Killing process with pid=%d\n",
            proc->pid);
        dump_scause();
        dump_process_info(proc);
        printk("\n");
        proc_set_killed(proc);
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

/// return to user space
void return_to_user_mode()
{
    // we're about to switch the destination of traps from
    // kernel_mode_interrupt_handler() to user_mode_interrupt_handler(), so turn
    // off interrupts until we're back in user space, where
    // user_mode_interrupt_handler() is correct.
    cpu_disable_device_interrupts();

    struct process *proc = get_current();

    // send syscalls, interrupts, and exceptions to u_mode_trap_vector in
    // u_mode_trap_vector.S
    size_t trampoline_u_mode_trap_vector =
        TRAMPOLINE + (u_mode_trap_vector - trampoline);
    rv_write_csr_stvec(trampoline_u_mode_trap_vector);

    // set up trapframe values that u_mode_trap_vector will need when
    // the process next traps into the kernel.
    proc->trapframe->kernel_page_table =
        cpu_get_page_table();  // kernel page table
    proc->trapframe->kernel_sp =
        proc->kstack + PAGE_SIZE;  // process's kernel stack
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
    size_t satp = MAKE_SATP(proc->pagetable);

    // jump to return_to_user_mode_asm in u_mode_trap_vector.S at the top of
    // memory, which switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    size_t return_to_user_mode_asm_ptr =
        TRAMPOLINE + (return_to_user_mode_asm - trampoline);
    ((void (*)(size_t))return_to_user_mode_asm_ptr)(satp);
}

/// Interrupts and exceptions go here via s_mode_trap_vector,
/// on whatever the current kernel stack is.
void kernel_mode_interrupt_handler(size_t *stack)
{
    xlen_t sepc = rv_read_csr_sepc();
    xlen_t sstatus = rv_read_csr_sstatus();

    if ((sstatus & SSTATUS_SPP) == 0)
    {
        panic(
            "kernel_mode_interrupt_handler was *not* called from supervisor "
            "mode");
    }
    if (cpu_is_device_interrupts_enabled())
    {
        panic("kernel_mode_interrupt_handler: interrupts are still enabled");
    }

    xlen_t scause = rv_read_csr_scause();
    bool yield_process = false;

    if (scause == SCAUSE_SUPERVISOR_SOFTWARE_INTERRUPT ||
        scause == SCAUSE_SUPERVISOR_TIMER_INTERRUPT)
    {
        handle_timer_interrupt();
        yield_process = true;
    }
    else if (scause == SCAUSE_SUPERVISOR_EXTERNAL_INTERRUPT)
    {
        handle_plic_device_interrupt();
    }
    else
    {
        printk(
            "\nFatal: unhandled interrupt in "
            "kernel_mode_interrupt_handler()\n");
        dump_scause();
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
    rv_write_csr_sepc(sepc);
    rv_write_csr_sstatus(sstatus);
}

void handle_plic_device_interrupt()
{
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();
    bool irq_handled = false;

    // find device for this IRQ and call interrupt handler
    for (size_t i = 0; i < MAX_DEVICES; ++i)
    {
        struct Device *dev = g_devices[i];
        if (dev && irq == dev->irq_number)
        {
            dev->dev_ops.interrupt_handler();
            irq_handled = true;
            break;
        }
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
#ifdef __ENABLE_SBI__
    // if the kernel runs in an SBI environment, the timer for the next
    // interrupt needs to be set here from S-Mode. If the kernel runs
    // bare-metal the M-Mode interrupt handler has already reset the timer
    // before triggering the S-Mode interrupt we are currently in (see
    // m_mode_trap_vector.S).
    sbi_set_timer();
#endif  // __ENABLE_SBI__

    // will only update on CPU 0
    if (smp_processor_id() == 0)
    {
        spin_lock(&g_tickslock);
        g_ticks++;
        wakeup(&g_ticks);
        spin_unlock(&g_tickslock);
    }

    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    rv_write_csr_sip(rv_read_csr_sip() & ~2);
}
