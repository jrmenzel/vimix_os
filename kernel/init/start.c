/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/start.h>
#include <init/dtb.h>
#include <init/main.h>
#include <init/start.h>
#include <kernel/kticks.h>
#include <kernel/string.h>
#include <kernel/vm.h>

/// entry.S needs one kernel stack per CPU (one page of 4KB each)
/// As long as the kernel stack is fixed at 4K, recursion can be deadly.
/// Placed in section STACK to not be placed in .bss (see kernel.ld).
/// This is because the .bss will be cleared by C code already relying on the
/// stack.
__attribute__((aligned(PAGE_SIZE))) __attribute__((
    section("STACK"))) char g_kernel_cpu_stack[KERNEL_STACK_SIZE * MAX_CPUS];

// All CPU threads start here after setting up the stack for C.
// The boot CPU runs first and starts all others explicitly,
// so there wont be race conditions checking g_global_init_done
void start(size_t cpuid, void *device_tree)
{
    cpu_set_boot_state();

    // disable paging for now.
    mmu_set_page_table(0, 0);

    // disable interrupts
    cpu_disable_interrupts();

    // clear BSS
    bool is_first_thread = (g_global_init_done == GLOBAL_INIT_NOT_STARTED);
    if (is_first_thread)
    {
        size_t size = (size_t)bss_end - (size_t)bss_start;
        memset((void *)bss_start, 0, size);
    }

    // define what interrupts should arrive, DOES NOT enable interrupts
    cpu_set_interrupt_mask();

    main(device_tree, is_first_thread);
}
