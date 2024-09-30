/* SPDX-License-Identifier: MIT */

#include "main.h"

#include <arch/interrupts.h>
#include <arch/platform.h>
#include <arch/trap.h>
#include <drivers/console.h>
#include <drivers/devices_list.h>
#include <drivers/ramdisk.h>
#include <drivers/virtio_disk.h>
#include <init/dtb.h>
#include <kernel/bio.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/smp.h>
#include <mm/memlayout.h>

#ifdef RAMDISK_EMBEDDED
#include <ramdisk_fs.h>
#endif

// to get a string from the git version number define
#define str_from_define(s) str(s)
#define str(s) #s

/// let hart 0 (or the first hart in SBI mode) signal to other harts when
/// the init is done which should only run on one core
volatile size_t g_global_init_done = GLOBAL_INIT_NOT_STARTED;

/// @brief print some debug info during boot
void print_kernel_info()
{
    printk("%zdKB of Kernel code\n", (size_t)(size_of_text) / 1024);
    printk("%zdKB of read only data\n", (size_t)(size_of_rodata) / 1024);
    printk("%zdKB of data\n", (size_t)(size_of_data) / 1024);
    printk("%zdKB of bss / uninitialized data\n", (size_t)(size_of_bss) / 1024);

#if defined(__TIMER_SOURCE_CLINT)
    printk("Timer source: CLINT\n");
#elif defined(__TIMER_SOURCE_SSTC)
    printk("Timer source: Sstc extension\n");
#elif defined(__TIMER_SOURCE_SBI)
    printk("Timer source: SBI\n");
#endif
}

#ifdef __ENABLE_SBI__
#define FEATURE_STRING "(SBI enabled)"
#else
#define FEATURE_STRING "(bare metal)"
#endif

void print_memory_map(struct Minimal_Memory_Map *memory_map)
{
    printk("    RAM S: 0x%zx\n", memory_map->ram_start);
    printk(" KERNEL S: 0x%zx\n", memory_map->kernel_start);
#ifdef RAMDISK_EMBEDDED
    printk("RAMDISK S: 0x%zx\n", (size_t)ramdisk_fs);
    printk("RAMDISK E: 0x%zx\n", (size_t)ramdisk_fs + ramdisk_fs_size);
#endif
    printk(" KERNEL E: 0x%zx\n", memory_map->kernel_end);
    if (memory_map->dtb_file_start != 0)
    {
        printk("    DTB S: 0x%zx\n", memory_map->dtb_file_start);
        printk("    DTB E: 0x%zx\n", memory_map->dtb_file_end);
    }
    if (memory_map->initrd_begin != 0)
    {
        printk(" INITRD S: 0x%zx\n", memory_map->initrd_begin);
        printk(" INITRD E: 0x%zx\n", memory_map->initrd_end);
    }
    printk("    RAM E: 0x%zx\n", memory_map->ram_end);
}

/// some init that only one thread should perform while all others wait
void init_by_first_thread(void *dtb)
{
    // get list of available devices from the device tree:
    struct Devices_List *dev_list = get_devices_list();
    dtb_get_devices(dtb, dev_list);

    // init a way to print, also starts uart:
    init_device(&(dev_list->dev[0]));  ///< device 0 is the console
    printk_init();
    printk("\n");
    printk("VIMIX OS " _arch_bits_string " bit " FEATURE_STRING
           " kernel version " str_from_define(GIT_HASH) " is booting\n");
    print_kernel_info();
    printk("\n");
    // print_found_devices();
    // printk("\n");

    // init memory management:
    printk("init memory management...\n");
    struct Minimal_Memory_Map memory_map;
    dtb_get_memory(dtb, &memory_map);
    print_memory_map(&memory_map);
    kalloc_init(&memory_map);         // physical page allocator
    kvm_init(&memory_map, dev_list);  // create kernel page table,
                                      // memory map found devices

    // init processes, syscalls and interrupts:
    printk("init process and syscall support...\n");
    proc_init();  // process table
    trap_init();  // trap vectors

    // init filesystem:
    printk("init filesystem...\n");
    bio_init();    // buffer cache
    inode_init();  // inode table
    file_init();   // file table

    printk("init remaining devices...\n");
    // device 0 is the console and was done already, now the rest:
    for (size_t i = 1; i < dev_list->dev_array_length; ++i)
    {
        init_device(&(dev_list->dev[i]));
    }

    // if a virtio device was found, init the one with the lowest address
    // this is the first device in qemu and the one the FS image is loaded
    ssize_t device_of_root_fs = -1;
    ssize_t i = get_first_virtio(dev_list);
    if (i >= 0)
    {
        device_of_root_fs = i;
    }
    if (memory_map.initrd_begin != 0)
    {
        dev_list->dev[1].mapping.mem_start = memory_map.initrd_begin;
        dev_list->dev[1].mapping.mem_size =
            memory_map.initrd_end - memory_map.initrd_begin;
        dev_list->dev[1].found = true;
        device_of_root_fs = 1;
    }
#ifdef RAMDISK_EMBEDDED
    // a ram disk has preference over a previously found virtio disk
    device_of_root_fs = 2;
#endif

    // store the device number of root:
    if (device_of_root_fs != -1)
    {
        ROOT_DEVICE_NUMBER = dev_list->dev[device_of_root_fs].dev_num;
        printk("fs root device: %s (%d,%d)\n",
               dev_list->dev[device_of_root_fs].dtb_name,
               MAJOR(ROOT_DEVICE_NUMBER), MINOR(ROOT_DEVICE_NUMBER));
    }
    else
    {
        panic("NO ROOT FILESYSTEM FOUND");
    }
    // e.g. boots other harts in SBI mode:
    init_platform();

    // process 0:
    printk("init userspace...\n");
    userspace_init();  // first user process

#ifdef CONFIG_DEBUG_KALLOC
    printk("Memory used: %zdkb - %zdkb free\n",
           kalloc_debug_get_allocation_count() * 4,
           kalloc_get_free_memory() / 1024);
#else
    printk("%zdkb free\n", kalloc_get_free_memory() / 1024);
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
        init_by_first_thread(device_tree);
    }
    else
    {
        while (g_global_init_done != GLOBAL_INIT_DONE)
        {
            __sync_synchronize();
        }
    }
    printk("hart %zd starting %s\n", smp_processor_id(),
           (is_first_thread ? "(init hart)" : ""));

    kvm_init_per_cpu();        // turn on paging
    set_s_mode_trap_vector();  // install kernel trap vector
    init_interrupt_controller_per_hart();

    scheduler();
}
