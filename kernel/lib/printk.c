/* SPDX-License-Identifier: MIT */

//
// formatted console output -- printk, panic.
//

#include <drivers/console.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/reset.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/stdarg.h>
#include <kernel/types.h>

#include "print_impl.h"

volatile size_t g_kernel_panicked = 0;

/// lock to avoid interleaving concurrent printk's.
static struct
{
    struct spinlock lock;
    bool locking;
} g_printk;

void console_putc_dummy(int32_t c, size_t payload) { console_putc(c); }

// Print to the console
void printk(char* format, ...)
{
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
    g_kernel_panicked++;  // freeze scheduling on other CPUs
    __sync_synchronize();
    cpu_disable_device_interrupts();

    printk("\n\nKernel PANIC on CPU %zd: %s\n", smp_processor_id(),
           error_message);
    if (g_kernel_panicked > 1)
    {
        if (g_kernel_panicked > 2)
        {
            // machine_power_off failed before and panicked
            while (true)
            {
            }
        }
        machine_power_off();
    }

#if defined(_ARCH_riscv)
    // print the kernel call stack:
    size_t depth = 32;  // limit just in case of a corrupted stack
    printk(" Kernel call stack:\n");
    size_t frame_pointer = (size_t)__builtin_frame_address(0);
    while (depth-- != 0)
    {
        size_t ra = *((size_t*)(frame_pointer - 1 * sizeof(size_t)));
        frame_pointer = *((size_t*)(frame_pointer - 2 * sizeof(size_t)));
        printk("  ra: " FORMAT_REG_SIZE "\n", ra);
        if (frame_pointer == 0) break;
    };

    struct cpu* this_cpu = get_cpu();
    if (this_cpu && this_cpu->proc)
    {
        struct process* proc = this_cpu->proc;
        printk(" Process %s (PID: %d) call stack:\n", proc->name, proc->pid);
        debug_print_call_stack_user(proc);
    }
#endif

#if defined(_SHUTDOWN_ON_PANIC)
    machine_power_off();
#else
    while (true)
    {
        // allows other CPUs to react to console input and print more machine
        // state for debugging
    }
#endif
}

void printk_init()
{
    spin_lock_init(&g_printk.lock, "pr");
    g_printk.locking = true;
}
