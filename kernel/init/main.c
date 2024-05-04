/* SPDX-License-Identifier: MIT */

#include "main.h"

#include <arch/interrupts.h>
#include <arch/platform.h>
#include <arch/trap.h>
#include <drivers/console.h>
#include <drivers/dev_null.h>
#include <drivers/dev_zero.h>
#include <drivers/virtio_disk.h>
#include <kernel/bio.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/smp.h>
#include <mm/memlayout.h>

// to get a string from the git version number define
#define str_from_define(s) str(s)
#define str(s) #s

/// let hart 0 (or the first hart in SBI mode) signal to other harts when the
/// init is done which should only run on one core
volatile size_t g_global_init_done = GLOBAL_INIT_NOT_STARTED;

/// @brief print some debug info during boot
void print_kernel_info()
{
    printk("%dKB of Kernel code\n", (size_t)(size_of_text) / 1024);
    printk("%dKB of read only data\n", (size_t)(size_of_rodata) / 1024);
    printk("%dKB of data\n", (size_t)(size_of_data) / 1024);
    printk("%dKB of bss / uninitialized data\n", (size_t)(size_of_bss) / 1024);
}

#ifdef __ENABLE_SBI__
#define FEATURE_STRING "(SBI enabled)"
#else
#define FEATURE_STRING "(bare metal)"
#endif

/// some init that only one thread should perform while all others wait
void init_by_first_thread()
{
    // init a way to print, also starts uart:
    console_init();
    printk_init();
    printk("\n");
    printk("VIMIX OS " _arch_bits_string " bit " FEATURE_STRING
           " kernel version " str_from_define(GIT_HASH) " is booting\n");
    print_kernel_info();
    printk("\n");

    // e.g. boots other harts in SBI mode:
    init_platform();

    // init memory management:
    printk("init memory management...\n");
    kalloc_init(end_of_kernel, (char *)PHYSTOP);  // physical page allocator
    kvm_init((char *)PHYSTOP);                    // create kernel page table

    // init processes, syscalls and interrupts:
    printk("init process and syscall support...\n");
    proc_init();  // process table
    trap_init();  // trap vectors
    init_interrupt_controller();

    // init filesystem:
    printk("init filesystem...\n");
    bio_init();                               // buffer cache
    inode_init();                             // inode table
    file_init();                              // file table
    ROOT_DEVICE_NUMBER = virtio_disk_init();  // emulated hard disk
    // file system init will happen when the first process gets forked below
    // in userspace init.

    // init additional devices
    dev_null_init();
    dev_zero_init();

    // process 0:
    printk("init userspace...\n");
    userspace_init();  // first user process

#ifdef CONFIG_DEBUG_KALLOC
    printk("Memory used: %dkb - %dkb free\n",
           kalloc_debug_get_allocation_count() * 4,
           kalloc_get_free_memory() / 1024);
#else
    printk("%dkb free\n", kalloc_get_free_memory() / 1024);
#endif  // CONFIG_DEBUG_KALLOC

    // full memory barrier (gcc buildin):
    __sync_synchronize();
    g_global_init_done = GLOBAL_INIT_DONE;
}

/// start() jumps here in supervisor mode on all CPUs.
void main(void *device_tree, size_t is_first_thread)
{
    if (is_first_thread)
    {
        init_by_first_thread();
    }
    else
    {
        while (g_global_init_done != GLOBAL_INIT_DONE)
        {
            __sync_synchronize();
        }
    }
    printk("hart %d starting %s\n", smp_processor_id(),
           (is_first_thread ? "(init hart)" : ""));

    kvm_init_per_cpu();        // turn on paging
    set_s_mode_trap_vector();  // install kernel trap vector
    init_interrupt_controller_per_hart();

    scheduler();
}
