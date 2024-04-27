/* SPDX-License-Identifier: MIT */

#include "main.h"

#include <arch/interrupts.h>
#include <arch/platform.h>
#include <arch/trap.h>
#include <drivers/virtio_disk.h>
#include <kernel/bio.h>
#include <kernel/console.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/smp.h>
#include <mm/memlayout.h>

/// let hart 0 (or the first hart in SBI mode) signal to other harts when the
/// init is done which should only run on one core
volatile size_t g_global_init_done = GLOBAL_INIT_NOT_STARTED;

/// start() jumps here in supervisor mode on all CPUs.
void main(void *device_tree, size_t is_first_thread)
{
#ifdef __ENABLE_SBI__
    // On SBI one hart boots initially (it can be any hart ID).
    // Other harts are started later but they all call this main().
    // So only the first one will be the one performing the global init
    // (there is no race condition, all later harts are started by the
    // first one explicitly in init_platform())
    bool perform_global_init = (g_global_init_done == GLOBAL_INIT_NOT_STARTED);
#define FEATURE_STRING "(SBI enabled)"
#else
    // On bare metal, we pick hart 0 for the global init
    bool perform_global_init = (smp_processor_id() == 0);
#define FEATURE_STRING "(bare metal)"
#endif  // __ENABLE_SBI__

    if (perform_global_init)
    {
        // this init code is executed on one hart while the others wait:
        g_global_init_done = GLOBAL_INIT_IN_PROCESS;

        // init a way to print, also starts uart:
        console_init();
        printk_init();
        printk("\n");
        printk("VIMIX OS " _arch_bits_string " bit " FEATURE_STRING
               " is booting\n");
        printk("\n");

        // e.g. boots other harts in SBI mode:
        init_platform();

        // init memory management:
        kalloc_init(end_of_kernel, (char *)PHYSTOP);  // physical page allocator
        kvm_init((char *)PHYSTOP);  // create kernel page table
        kvm_init_per_cpu();         // turn on paging

        // init processes, syscalls and interrupts:
        proc_init();               // process table
        trap_init();               // trap vectors
        set_s_mode_trap_vector();  // install kernel trap vector
        init_interrupt_controller();
        init_interrupt_controller_per_hart();

        // init filesystem:
        bio_init();          // buffer cache
        inode_init();        // inode table
        file_init();         // file table
        virtio_disk_init();  // emulated hard disk

        // process 0:
        userspace_init();  // first user process

        printk("hart %d starting after init...\n", smp_processor_id());

        // full memory barrier (gcc buildin):
        __sync_synchronize();
        g_global_init_done = GLOBAL_INIT_DONE;
    }
    else
    {
        while (g_global_init_done != GLOBAL_INIT_DONE)
        {
            __sync_synchronize();
        }
        printk("hart %d starting\n", smp_processor_id());
    }

    kvm_init_per_cpu();        // turn on paging
    set_s_mode_trap_vector();  // install kernel trap vector
    init_interrupt_controller_per_hart();

    scheduler();
}
