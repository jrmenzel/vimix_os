/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <arch/riscv/asm/registers.h>
#include <arch/riscv/sbi.h>
#include <arch/trap.h>
#include <drivers/device.h>
#include <drivers/uart16550.h>
#include <drivers/virtio_disk.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

// each call to the timer interrupt is one tick
struct spinlock g_tickslock;
size_t g_ticks;

extern char trampoline[], u_mode_trap_vector[], return_to_user_mode_asm[];

/// in s_mode_trap_vector.S, calls kernel_mode_interrupt_handler().
extern void s_mode_trap_vector();

enum interrupt_source
{
    INTERRUPT_SOURCE_NOT_RECOGNIZED = 0,
    INTERRUPT_SOURCE_OTHER_DEVICE = 1,
    INTERRUPT_SOURCE_TIMER = 2,
    INTERRUPT_SOURCE_SYSCALL = 3
};
enum interrupt_source interrupt_handler();

void trap_init()
{
    g_ticks = 0;
    spin_lock_init(&g_tickslock, "time");
}

void set_s_mode_trap_vector()
{
    cpu_set_s_mode_trap_vector(s_mode_trap_vector);
}

#define SCAUSE_INSTRUCTION_ADDR_MISALIGN 0
#define SCAUSE_INSTRUCTION_ACCESS_FAULT 1
#define SCAUSE_ILLEGAL_INSTRUCTION 2
#define SCAUSE_BREAKPOINT 3
#define SCAUSE_LOAD_ADDR_MISALIGNED 4
#define SCAUSE_LOAD_ACCESS_FAULT 5
#define SCAUSE_STORE_AMO_ADDR_MISALIGN 6
#define SCAUSE_STORE_AMO_ACCESS_FAULT 7
#define SCAUSE_ECALL_FROM_U_MODE 8
#define SCAUSE_ECALL_FROM_S_MODE 9
#define SCAUSE_INSTRUCTION_PAGE_FAULT 12
#define SCAUSE_LOAD_PAGE_FAULT 13
#define SCAUSE_STORE_AMO_PAGE_FAULT 15

const char *scause_exception_code_to_string(xlen_t scause)
{
    switch (scause)
    {
        case SCAUSE_INSTRUCTION_ADDR_MISALIGN:
            return "instuction address misaligned";
        case SCAUSE_INSTRUCTION_ACCESS_FAULT: return "instruction access fault";
        case SCAUSE_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case SCAUSE_BREAKPOINT: return "breakpoint";
        case SCAUSE_LOAD_ADDR_MISALIGNED: return "load address misaligned";
        case SCAUSE_LOAD_ACCESS_FAULT: return "load access fault";
        case SCAUSE_STORE_AMO_ADDR_MISALIGN:
            return "store/AMO address misaligned";
        case SCAUSE_STORE_AMO_ACCESS_FAULT: return "store/AMO access fault";
        case SCAUSE_ECALL_FROM_U_MODE: return "environment call from U-mode";
        case SCAUSE_ECALL_FROM_S_MODE: return "environment call from S-mode";
        case 10: return "reserved";
        case 11: return "reserved";
        case SCAUSE_INSTRUCTION_PAGE_FAULT: return "instruction page fault";
        case SCAUSE_LOAD_PAGE_FAULT: return "load page fault";
        case 14: return "reserved";
        case SCAUSE_STORE_AMO_PAGE_FAULT: return "store/AMO page fault";
        default: return "unknown scause";
    };
}

void dump_scause()
{
    xlen_t scause = rv_read_csr_scause();
    xlen_t stval = rv_read_csr_stval();

    printk("scause (0x%p): %s\n", scause,
           scause_exception_code_to_string(scause));
    printk("sepc=0x%p stval=0x%p\n", rv_read_csr_sepc(), stval);

    if (scause == SCAUSE_INSTRUCTION_PAGE_FAULT ||
        scause == SCAUSE_LOAD_PAGE_FAULT ||
        scause == SCAUSE_STORE_AMO_PAGE_FAULT)
    {
        // stval is set to the offending memory address
        if (stval == 0)
        {
            printk("Dereferenced NULL pointer\n");
        }
        else
        {
            size_t offset = stval % PAGE_SIZE;
            size_t page_start = stval - offset;
            printk("Tried to access address 0x%p (offset 0x%p in page 0x%p)\n",
                   stval, offset, page_start);
        }
    }
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

    struct process *proc = get_current();

    // save user program counter.
    trapframe_set_program_counter(proc->trapframe, rv_read_csr_sepc());

    enum interrupt_source int_src = interrupt_handler();
    if (rv_read_csr_scause() == 8)
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
    else if (int_src != INTERRUPT_SOURCE_NOT_RECOGNIZED)
    {
        // ok
    }
    else
    {
        printk("user_mode_interrupt_handler(): unexpected scause for pid=%d\n",
               proc->pid);
        dump_scause();
        proc_set_killed(proc);
    }

    if (proc_is_killed(proc))
    {
        exit(-1);
    }

    // give up the CPU if this is a timer interrupt.
    if (int_src == INTERRUPT_SOURCE_TIMER)
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
            "kernel_mode_interrupt_handler called *not* from supervisor mode");
    }
    if (cpu_is_device_interrupts_enabled())
    {
        panic("kernel_mode_interrupt_handler: interrupts are still enabled");
    }

    enum interrupt_source int_src = interrupt_handler();

    if (int_src == INTERRUPT_SOURCE_NOT_RECOGNIZED)
    {
        printk(
            "\nFatal: unhandled interrupt in "
            "kernel_mode_interrupt_handler()\n");
        dump_scause();
        dump_pre_int_kthread_state(stack);
        panic("kernel_mode_interrupt_handler");
    }

    if (int_src == INTERRUPT_SOURCE_TIMER)
    {
        // give up the CPU
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

void update_kernel_ticks()
{
    if (smp_processor_id() != 0) return;

    spin_lock(&g_tickslock);
    g_ticks++;
    wakeup(&g_ticks);
    spin_unlock(&g_tickslock);
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

#if defined(_arch_is_32bit)
const size_t SCAUSE_OTHER_DEVICE = 0x80000000L;
const size_t SCAUSE_TIMER_DEVICE = 0x80000001L;
const size_t SCAUSE_TIMER_EVENT = 0x80000005L;  // timer from SBI
#else
const size_t SCAUSE_OTHER_DEVICE = 0x8000000000000000L;
const size_t SCAUSE_TIMER_DEVICE = 0x8000000000000001L;  // timer from M-Mode
const size_t SCAUSE_TIMER_EVENT = 0x8000000000000005L;   // timer from SBI
#endif

/// check if it's an external interrupt or software interrupt,
/// and handle it.
enum interrupt_source interrupt_handler()
{
    xlen_t scause = rv_read_csr_scause();

    if ((scause & SCAUSE_OTHER_DEVICE) && (scause & 0xff) == 9)
    {
        handle_plic_device_interrupt();
        return INTERRUPT_SOURCE_OTHER_DEVICE;
    }
    else if (scause == SCAUSE_TIMER_DEVICE || scause == SCAUSE_TIMER_EVENT)
    {
#ifdef __ENABLE_SBI__
        // if the kernel runs in an SBI environment, the timer for the next
        // interrupt needs to be set here from S-Mode. If the kernel runs
        // bare-metal the M-Mode interrupt handler has already reset the timer
        // before triggering the S-Mode interrupt we are currently in (see
        // m_mode_trap_vector.S).
        sbi_set_timer();
#endif  // __ENABLE_SBI__
        if (smp_processor_id() == 0)
        {
            update_kernel_ticks();
        }

        // acknowledge the software interrupt by clearing
        // the SSIP bit in sip.
        rv_write_csr_sip(rv_read_csr_sip() & ~2);

        return INTERRUPT_SOURCE_TIMER;
    }

    return INTERRUPT_SOURCE_NOT_RECOGNIZED;
}
