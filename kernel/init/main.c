/* SPDX-License-Identifier: MIT */

#include <arch/riscv/plic.h>
#include <arch/trap.h>
#include <drivers/virtio_disk.h>
#include <kernel/bio.h>
#include <kernel/console.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

volatile static size_t g_global_init_done = 0;

/// start() jumps here in supervisor mode on all CPUs.
void main()
{
    if (smp_processor_id() == 0)
    {
        // this init code is executed on one hart while the others wait:

        // init a way to print, also starts uart:
        console_init();
        printk_init();
        printk("\n");
        printk("VIMIX kernel is booting\n");
        printk("\n");

        // init memory management:
        kalloc_init();       // physical page allocator
        kvm_init();          // create kernel page table
        kvm_init_per_cpu();  // turn on paging

        // init processes, syscalls and interrupts:
        proc_init();          // process table
        trap_init();          // trap vectors
        trap_init_per_cpu();  // install kernel trap vector
        plic_init();          // set up interrupt controller
        plic_init_per_cpu();  // ask PLIC for device interrupts

        // init filesystem:
        bio_init();          // buffer cache
        inode_init();        // inode table
        file_init();         // file table
        virtio_disk_init();  // emulated hard disk

        // process 0:
        userspace_init();  // first user process

        // full memory barrier (gcc buildin):
        __sync_synchronize();
        g_global_init_done = 1;
    }
    else
    {
        while (g_global_init_done == 0)
        {
            __sync_synchronize();
        }
        printk("hart %d starting\n", smp_processor_id());
        kvm_init_per_cpu();   // turn on paging
        trap_init_per_cpu();  // install kernel trap vector
        plic_init_per_cpu();  // ask PLIC for device interrupts
    }

    scheduler();
}
