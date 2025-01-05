/* SPDX-License-Identifier: MIT */

#include "main.h"

#include <arch/interrupts.h>
#include <arch/platform.h>
#include <arch/trap.h>
#include <drivers/console.h>
#include <drivers/dev_null.h>
#include <drivers/dev_zero.h>
#include <drivers/devices_list.h>
#include <drivers/ramdisk.h>
#include <drivers/virtio_disk.h>
#include <fs/vfs.h>
#include <init/dtb.h>
#include <kernel/bio.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/smp.h>

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
    // printk("    BSS S: 0x%zx\n", (size_t)bss_start);
    // printk("    BSS E: 0x%zx\n", (size_t)bss_end);
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
    size_t ram_size_mb =
        (memory_map->ram_end - memory_map->ram_start) / (1024 * 1024);
    printk("    RAM E: 0x%zx - size: %zd MB\n", memory_map->ram_end,
           ram_size_mb);
}

void add_ramdisks_to_dev_list(struct Devices_List *dev_list,
                              struct Minimal_Memory_Map *memory_map)
{
    struct Device_Init_Parameters init_params;
#ifdef RAMDISK_EMBEDDED
    clear_init_parameters(&init_params);
    init_params.mem_start = (size_t)ramdisk_fs;
    init_params.mem_size = (size_t)ramdisk_fs_size;
    dev_list_add_with_parameters(dev_list, "ramdisk_embedded", ramdisk_init,
                                 init_params);
#endif
    if (memory_map->initrd_begin != 0)
    {
        // boot loader provided ramdisk detected
        clear_init_parameters(&init_params);
        init_params.mem_start = memory_map->initrd_begin;
        init_params.mem_size =
            memory_map->initrd_end - memory_map->initrd_begin;
        dev_list_add_with_parameters(dev_list, "ramdisk_initrd", ramdisk_init,
                                     init_params);
    }
}

/// some init that only one thread should perform while all others wait
void init_by_first_thread(void *dtb)
{
    // Collect all found devices in this list for later init:
    struct Devices_List *dev_list = get_devices_list();

    // init a way to print, starts uart (unless the SBI console is used):
    ssize_t console_index = dtb_add_boot_console_to_dev_list(dtb, dev_list);
    if (console_index >= 0)
    {
        init_device(&(dev_list->dev[console_index]));
    }
    else
    {
        // fallback if no UART was found: try the SBI console:
        console_init(NULL, NULL);
    }
    printk_init();

    printk("\n");
    printk("VIMIX OS " _arch_bits_string " bit " FEATURE_STRING
           " kernel version " str_from_define(GIT_HASH) " is booting\n");
    print_kernel_info();
    if (console_index < 0)
    {
        printk("Console: SBI\n");
    }
    else
    {
        printk("Console: %s\n", dev_list->dev[console_index].dtb_name);
    }

    struct Minimal_Memory_Map memory_map;
    dtb_get_memory(dtb, &memory_map);

    // collect all found devices:
    add_ramdisks_to_dev_list(dev_list, &memory_map);
    dev_list_add(dev_list, "/dev/null", dev_null_init);
    dev_list_add(dev_list, "/dev/zero", dev_zero_init);
    dtb_add_devices_to_dev_list(dtb, get_generell_drivers(), dev_list);
    dev_list_sort(dev_list,
                  "virtio,mmio");  // for predictable dev numbers on qemu
    // debug_dev_list_print(dev_list);

    // init memory management:
    printk("init memory management...\n");

#ifdef _PLATFORM_VISIONFIVE2
    // cap usable memory for performance reasons if MEMORY_SIZE is set
    size_t ram_size = memory_map.ram_end - memory_map.ram_start;
    size_t max_ram = 1024 * 1024 * MEMORY_SIZE;
    if ((max_ram != 0) && (ram_size > max_ram))
    {
        memory_map.ram_end = memory_map.ram_start + max_ram;
    }
#endif

    print_memory_map(&memory_map);
    kalloc_init(&memory_map);         // physical page allocator
    kvm_init(&memory_map, dev_list);  // create kernel page table,
                                      // memory map found devices

    // init processes, syscalls and interrupts:
    printk("init process and syscall support...\n");
    proc_init();  // process table

    // init filesystem:
    printk("init filesystem...\n");
    bio_init();  // buffer cache
    init_virtual_file_system();
    file_init();  // file table

    printk("init remaining devices...\n");
    dev_list_init_all_devices(dev_list);

    // find the device with the root file system:
    size_t device_of_root_fs = 0;
    ssize_t ramdisk_index_0 =
        dev_list_get_device_index(dev_list, "ramdisk_embedded");
    ssize_t ramdisk_index_1 =
        dev_list_get_device_index(dev_list, "ramdisk_initrd");
    ssize_t disk_index_0 =
        dev_list_get_first_device_index(dev_list, "virtio,mmio");
    if (ramdisk_index_0 >= 0)
    {
        device_of_root_fs = ramdisk_index_0;
    }
    else if (ramdisk_index_1 >= 0)
    {
        device_of_root_fs = ramdisk_index_1;
    }
    else if (disk_index_0 >= 0)
    {
        device_of_root_fs = disk_index_0;
    }
    else
    {
        panic("NO ROOT FILESYSTEM FOUND");
    }

    // store the device number of root:
    ROOT_DEVICE_NUMBER = dev_list->dev[device_of_root_fs].dev_num;
    printk("fs root device: %s (%d,%d)\n",
           dev_list->dev[device_of_root_fs].dtb_name, MAJOR(ROOT_DEVICE_NUMBER),
           MINOR(ROOT_DEVICE_NUMBER));

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
