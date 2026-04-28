/* SPDX-License-Identifier: MIT */

//
// panic handling.
//

#include <fs/fs_lookup.h>
#include <init/start.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/reset.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/stdatomic.h>
#include <kernel/string.h>
#include <kernel/types.h>
#include <lib/panic.h>
#include <lib/xdbg/xdbg.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>
#include <syscalls/syscall.h>

atomic_size_t g_kernel_panicked = 0;

struct debug_info *g_kernel_debug_info = NULL;

void panic(char *error_message)
{
    printk_disable_locking();
    atomic_fetch_add(&g_kernel_panicked, 1);
    atomic_thread_fence(memory_order_seq_cst);
    cpu_disable_interrupts();
    struct cpu *this_cpu = get_cpu();
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

    if (g_kernel_init_status >= KERNEL_INIT_FULLY_BOOTED)
    {
        // stop other CPUs
        cpu_mask mask = ipi_cpu_mask_all_but_self();
        ipi_send_interrupt(mask, IPI_KERNEL_PANIC, NULL);
    }

#if defined(__ARCH_riscv)
    printk("kernel call stack:\n");
    debug_print_call_stack_kernel_fp((size_t)__builtin_frame_address(0));

    if (this_cpu->proc)
    {
        struct process *proc = this_cpu->proc;
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

void debug_print_pc(size_t pc)
{
    if (g_kernel_debug_info != NULL)
    {
        // try to find calling instruction,
        // can be 2 bytes or 4 bytes before return address
        const struct instruction *instr;
        instr = debug_info_lookup_instruction(g_kernel_debug_info, pc);
        debug_info_print_instruction(g_kernel_debug_info, instr);
    }
}

void debug_print_ra(size_t ra)
{
    printk("ra: " FORMAT_REG_SIZE " ", ra);

    if (g_kernel_debug_info != NULL)
    {
        // try to find calling instruction,
        // can be 2 bytes or 4 bytes before return address
        const struct instruction *instr;
        // instr = debug_info_lookup_instruction(g_kernel_debug_info, ra);
        // debug_info_print_instruction(g_kernel_debug_info, instr);
        printk(" caller: ");
        instr = debug_info_lookup_caller(g_kernel_debug_info, ra);
        debug_info_print_instruction(g_kernel_debug_info, instr);
    }

    printk("\n");
}

void debug_print_call_stack_kernel_fp(size_t frame_pointer)
{
    size_t depth = 32;  // limit just in case of a corrupted stack
    while (depth-- != 0)
    {
        if (frame_pointer == 0) break;
        size_t ra_address = frame_pointer - 1 * sizeof(size_t);
        // check if g_kernel_pagetable is set, kernel panics could happen before
        // that
        if (g_kernel_pagetable->root && kvm_get_physical_paddr(ra_address) == 0)
        {
            printk("  ra: <invalid address>\n");
            break;
        }
        size_t ra = *((size_t *)(ra_address));
        size_t next_frame_pointer_address = frame_pointer - 2 * sizeof(size_t);
        if (g_kernel_pagetable->root &&
            kvm_get_physical_paddr(next_frame_pointer_address) == 0)
        {
            printk("  invalid frame pointer address: 0x%zx\n",
                   next_frame_pointer_address);
            break;
        }
        frame_pointer = *((size_t *)(next_frame_pointer_address));
        printk("  ");
        debug_print_ra(ra);
        if (ADDR_IS_TRAMPOLINE(ra))
        {
            // reached trampoline code
            break;
        }
    };
}

syserr_t panic_load_debug_symbols(const char *debug_file_path)
{
    syserr_t error = 0;
    struct dentry *dp = dentry_from_path(debug_file_path, &error);
    if (dp == NULL)
    {
        return error;
    }
    if (dentry_is_invalid(dp))
    {
        dentry_put(dp);
        return -ENOENT;
    }

    struct debug_info *xdbg_info = debug_info_alloc();
    if (xdbg_info == NULL)
    {
        dentry_put(dp);
        return -ENOMEM;
    }

    inode_lock(dp->ip);
    syserr_t ret = debug_info_read(xdbg_info, dp);
    if (ret < 0)
    {
        printk("Loading kernel debug symbols from %s failed.\n",
               debug_file_path);
        debug_info_free(xdbg_info);
    }
    else
    {
        if (g_kernel_debug_info != NULL)
        {
            debug_info_free(g_kernel_debug_info);
        }
        g_kernel_debug_info = xdbg_info;
        printk("Loaded kernel debug symbols from %s.\n", debug_file_path);
    }
    inode_unlock(dp->ip);
    dentry_put(dp);

    return ret;
}
