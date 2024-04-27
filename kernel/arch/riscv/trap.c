/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/riscv/plic.h>
#include <arch/trap.h>
#include <drivers/uart16550.h>
#include <drivers/virtio_disk.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

// each call to the timer interrupt is one tick
struct spinlock g_tickslock;
size_t g_ticks;

extern char trampoline[], u_mode_trap_vector[], return_to_user_mode_asm[];

/// in s_mode_trap_vector.S, calls kernel_mode_interrupt_handler().
void s_mode_trap_vector();

extern int interrupt_handler();

void trap_init() { spin_lock_init(&g_tickslock, "time"); }

/// set up to take exceptions and traps while in the kernel.
void trap_init_per_cpu() { w_stvec((xlen_t)s_mode_trap_vector); }

/// handle an interrupt, exception, or system call from user space.
/// called from u_mode_trap_vector.S
void user_mode_interrupt_handler()
{
    int which_dev = 0;

    if ((r_sstatus() & SSTATUS_SPP) != 0)
        panic("user_mode_interrupt_handler: not from user mode");

    // send interrupts and exceptions to kernel_mode_interrupt_handler(),
    // since we're now in the kernel.
    w_stvec((uint64_t)s_mode_trap_vector);

    struct process *proc = get_current();

    // save user program counter.
    proc->trapframe->epc = r_sepc();

    if (r_scause() == 8)
    {
        // system call

        if (proc_is_killed(proc)) exit(-1);

        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        proc->trapframe->epc += 4;

        // an interrupt will change sepc, scause, and sstatus,
        // so enable only now that we're done with those registers.
        cpu_enable_device_interrupts();

        syscall();
    }
    else if ((which_dev = interrupt_handler()) != 0)
    {
        // ok
    }
    else
    {
        printk("user_mode_interrupt_handler(): unexpected scause %p pid=%d\n",
               r_scause(), proc->pid);
        printk("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        proc_set_killed(proc);
    }

    if (proc_is_killed(proc)) exit(-1);

    // give up the CPU if this is a timer interrupt.
    if (which_dev == 2) yield();

    return_to_user_mode();
}

/// return to user space
void return_to_user_mode()
{
    struct process *proc = get_current();

    // we're about to switch the destination of traps from
    // kernel_mode_interrupt_handler() to user_mode_interrupt_handler(), so turn
    // off interrupts until we're back in user space, where
    // user_mode_interrupt_handler() is correct.
    cpu_disable_device_interrupts();

    // send syscalls, interrupts, and exceptions to u_mode_trap_vector in
    // u_mode_trap_vector.S
    size_t trampoline_u_mode_trap_vector =
        TRAMPOLINE + (u_mode_trap_vector - trampoline);
    w_stvec(trampoline_u_mode_trap_vector);

    // set up trapframe values that u_mode_trap_vector will need when
    // the process next traps into the kernel.
    proc->trapframe->kernel_page_table = r_satp();  // kernel page table
    proc->trapframe->kernel_sp =
        proc->kstack + PAGE_SIZE;  // process's kernel stack
    proc->trapframe->kernel_trap = (size_t)user_mode_interrupt_handler;
    proc->trapframe->kernel_hartid =
        __arch_smp_processor_id();  // hartid for smp_processor_id()

    // set up the registers that u_mode_trap_vector.S's sret will use
    // to get to user space.

    // set S Previous Privilege mode to User.
    xlen_t x = r_sstatus();
    x &= ~SSTATUS_SPP;  // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE;  // enable interrupts in user mode
    w_sstatus(x);

    // set S Exception Program Counter to the saved user pc.
    w_sepc(proc->trapframe->epc);

    // tell u_mode_trap_vector.S the user page table to switch to.
    size_t satp = MAKE_SATP(proc->pagetable);

    // jump to return_to_user_mode_asm in u_mode_trap_vector.S at the top of
    // memory, which switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    size_t trampoline_userret =
        TRAMPOLINE + (return_to_user_mode_asm - trampoline);
    ((void (*)(size_t))trampoline_userret)(satp);
}

/// Interrupts and exceptions go here via s_mode_trap_vector,
/// on whatever the current kernel stack is.
void kernel_mode_interrupt_handler()
{
    int which_dev = 0;
    xlen_t sepc = r_sepc();
    xlen_t sstatus = r_sstatus();
    xlen_t scause = r_scause();

    if ((sstatus & SSTATUS_SPP) == 0)
        panic("kernel_mode_interrupt_handler: not from supervisor mode");
    if (cpu_is_device_interrupts_enabled() != 0)
        panic("kernel_mode_interrupt_handler: interrupts enabled");

    if ((which_dev = interrupt_handler()) == 0)
    {
        printk("scause %p\n", scause);
        printk("sepc=%p stval=%p\n", r_sepc(), r_stval());
        panic("kernel_mode_interrupt_handler");
    }

    // give up the CPU if this is a timer interrupt.
    if (which_dev == 2 && get_current() != 0 && get_current()->state == RUNNING)
        yield();

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by s_mode_trap_vector.S's sepc
    // instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
}

void clockintr()
{
    spin_lock(&g_tickslock);
    g_ticks++;
    wakeup(&g_ticks);
    spin_unlock(&g_tickslock);
}

/// check if it's an external interrupt or software interrupt,
/// and handle it.
/// returns 2 if timer interrupt,
/// 1 if other device,
/// 0 if not recognized.
int interrupt_handler()
{
    xlen_t scause = r_scause();

    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9)
    {
        // this is a supervisor external interrupt, via PLIC.

        // irq indicates which device interrupted.
        int irq = plic_claim();

        if (irq == UART0_IRQ)
        {
            uart_interrupt_handler();
        }
        else if (irq == VIRTIO0_IRQ)
        {
            virtio_block_device_interrupt();
        }
        else if (irq)
        {
            printk("unexpected interrupt irq=%d\n", irq);
        }

        // the PLIC allows each device to raise at most one
        // interrupt at a time; tell the PLIC the device is
        // now allowed to interrupt again.
        if (irq) plic_complete(irq);

        return 1;
    }
    else if (scause == 0x8000000000000001L)
    {
        // software interrupt from a machine-mode timer interrupt,
        // forwarded by m_mode_trap_vector in s_mode_trap_vector.S.

        if (smp_processor_id() == 0)
        {
            clockintr();
        }

        // acknowledge the software interrupt by clearing
        // the SSIP bit in sip.
        w_sip(r_sip() & ~2);

        return 2;
    }
    else
    {
        return 0;
    }
}
