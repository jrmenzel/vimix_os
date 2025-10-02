/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk, panic.
//

#include <drivers/console.h>
#include <init/main.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/reset.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/stdarg.h>
#include <kernel/stdatomic.h>
#include <kernel/types.h>
#include <syscalls/syscall.h>

#include "print_impl.h"

atomic_size_t g_kernel_panicked = 0;

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
    bool init;
} g_printk = {0};

void console_putc_dummy(int32_t c, size_t payload) { console_putc(c); }

// Print to the console
void printk(char* format, ...)
{
    if (g_printk.init == false)
    {
        // return;
    }

    // error checks
    if (format == NULL)
    {
        panic("null format in printk");
    }

    // lock
    bool locking = g_printk.locking;
    if (locking)
    {
        spin_lock(&g_printk.lock);
    }

    // printing via console_putc()
    va_list ap;
    va_start(ap, format);
    print_impl(console_putc_dummy, 0, format, ap);
    va_end(ap);

    // unlock
    if (locking)
    {
        spin_unlock(&g_printk.lock);
    }
}

void panic(char* error_message)
{
    g_printk.locking = false;
    atomic_fetch_add(&g_kernel_panicked, 1);
    atomic_thread_fence(memory_order_seq_cst);
    cpu_disable_interrupts();
    struct cpu* this_cpu = get_cpu();
    this_cpu->state = CPU_PANICKED;

    atomic_thread_fence(memory_order_seq_cst);

    printk("\n\nKernel PANIC on CPU %zd: %s\n", smp_processor_id(),
           error_message);
    if (atomic_load(&g_kernel_panicked) > 1)
    {
        if (atomic_load(&g_kernel_panicked) > 2)
        {
            // machine_power_off failed before and panicked
            infinite_loop;
        }
        machine_power_off();
    }

    if (g_global_init_done == GLOBAL_INIT_DONE)
    {
        // stop other CPUs
        cpu_mask mask = ipi_cpu_mask_all_but_self();
        ipi_send_interrupt(mask, IPI_KERNEL_PANIC, NULL);
    }

#if defined(__ARCH_riscv)
    // print the kernel call stack:
    debug_print_call_stack_kernel_fp((size_t)__builtin_frame_address(0));

    if (this_cpu->proc)
    {
        struct process* proc = this_cpu->proc;
        printk(" Process %s (PID: %d)", proc->name, proc->pid);
#ifdef CONFIG_DEBUG
        if (proc->current_syscall != 0)
        {
            printk(" in syscall %s()\n",
                   debug_get_syscall_name(proc->current_syscall));
        }
#else
        printk("\n");
#endif
        printk(" Call stack:\n");
        debug_print_call_stack_user(proc);
    }
#endif

#if defined(_SHUTDOWN_ON_PANIC)
    machine_power_off();
#endif

    // allows other CPUs to react to console input and print more machine
    // state for debugging
    infinite_loop;
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = true;
    g_printk.init = true;
}
