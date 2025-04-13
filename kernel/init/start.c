/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/start.h>
#include <arch/timer.h>
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

void wait_on_bss_clear(bool this_thread_clears)
{
    if (this_thread_clears)
    {
        // clears the .bss section with zeros
        size_t size = (size_t)bss_end - (size_t)bss_start;
        memset((void *)bss_start, 0, size);

        g_global_init_done = GLOBAL_INIT_BSS_CLEAR;
    }
    else
    {
        while (g_global_init_done < GLOBAL_INIT_BSS_CLEAR)
        {
            __sync_synchronize();
        }
    }
}

void start(size_t cpuid, void *device_tree)
{
    bool is_first_thread = is_first_thread(cpuid);

    cpu_set_boot_state();

    // disable paging for now.
    mmu_set_page_table(0, 0);

    // disable interrupts
    cpu_disable_interrupts();

    // clear BSS
    wait_on_bss_clear(is_first_thread);

    if (is_first_thread)
    {
        kticks_init();
    }
    uint64_t timebase = dtb_get_timebase(device_tree);
    timer_init(timebase, device_tree, cpuid);

    // define what interrupts should arrive, DOES NOT enable interrupts
    cpu_set_interrupt_mask();

    jump_to_main(device_tree, is_first_thread);
}
