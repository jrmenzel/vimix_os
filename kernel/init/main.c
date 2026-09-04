/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/system.h>
#include <arch/trap.h>
#include <drivers/bdev/ramdisk.h>
#include <drivers/bdev/virtio_disk.h>
#include <drivers/devices_list.h>
#include <drivers/driver_list.h>
#include <drivers/tty/console.h>
#include <init/dtb.h>
#include <init/early_pgtable.h>
#include <init/main.h>
#include <init/start.h>
#include <init/system.h>
#include <kernel/bio.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/interrupt_controller.h>
#include <kernel/kobject.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/smp.h>
#include <kernel/timer.h>
#include <mm/kalloc.h>
#include <mm/kernel_memory.h>
#include <mm/memlayout.h>
#include <mm/memory_map.h>
#include <mm/vm.h>

#if defined(__CONFIG_RAMDISK_EMBEDDED)
#include <ramdisk_fs.h>
#endif

// to get a string from the git version number define
#define str_from_define(s) str(s)
#define str(s) #s

void add_ramdisks_to_dev_list(const void *dtb, struct Devices_List *dev_list)
{
    struct Driver *ramdisk_driver = NULL;
    for_each_driver(driver)
    {
        if (strcmp(driver->dtb_name, "ramdisk") == 0)
        {
            ramdisk_driver = driver;
            break;
        }
    }
    if (ramdisk_driver == NULL)
    {
        return;
    }

#if defined(__CONFIG_RAMDISK_EMBEDDED)
    struct Device_Init_Parameters init_params;
    clear_init_parameters(&init_params);
    init_params.mem[0].start_pa = virt_to_phys((size_t)ramdisk_fs);
    init_params.mem[0].size = (size_t)ramdisk_fs_size;
    dev_list_add_with_parameters(dev_list, ramdisk_driver, init_params);
#endif

    // get initrd / ramdisk if present
    size_t initrd_base;
    size_t initrd_size;
    dtb_get_initrd(dtb, &initrd_base, &initrd_size);
    if (initrd_base != 0 && initrd_size != 0)
    {
        struct Device_Init_Parameters init_params;
        clear_init_parameters(&init_params);
        init_params.mem[0].start_pa = initrd_base;
        init_params.mem[0].size = initrd_size;
        dev_list_add_with_parameters(dev_list, ramdisk_driver, init_params);
    }
}

void init_devices(const void *dtb)
{
    printk("init devices list...\n");

    struct Devices_List *dev_list = get_devices_list();
    driver_list_init();
    dev_list_add_virtual_devices(dev_list);

    // Collect all found devices in this list for later init:
    dtb_add_devices_to_dev_list(dtb, dev_list);
    // add ramdisk if present:
    add_ramdisks_to_dev_list(g_system.boot_dtb, dev_list);

    // map devices
    memory_map_add_device_mmio(&g_kernel_pagetable->memory_map, dev_list);
    kvm_apply_kernel_mapping(g_kernel_pagetable);
    // now we are done with the page table: unlock
    spin_unlock(&g_kernel_pagetable->lock);

    // init a way to print, starts uart:
    struct Found_Device *console_dev =
        dtb_find_boot_console_in_dev_list(dtb, dev_list);

    if (console_dev == NULL)
    {
        panic("no console");
    }

    printk("init console: %s\n", console_dev->driver->dtb_name);
    dev_t con_dev = init_device(dev_list, console_dev);

    if (con_dev == INVALID_DEVICE)
    {
        panic("not a valid console");
    }

    printk("init remaining devices...\n");
    dev_list_init_all_devices(dev_list);
}

void init_memory_management(const void *dtb)
{
    printk("init early memory management...\n");
    g_kernel_memory.phys_base = g_early_memory_map.phys_base;

    struct MM_Region *early_ram =
        early_memory_map_get_region(&g_early_memory_map, MM_REGION_EARLY_RAM);
    kalloc_init(early_ram);  // physical page allocator

    // from now on kmalloc() can be used, but available memory is limited

    printk("init new page table...\n");
    g_kernel_pagetable = page_table_alloc_init();

    // get total RAM from dtb
    size_t phy_ram_base;
    size_t phy_ram_size;
    dtb_get_memory(dtb, &phy_ram_base, &phy_ram_size);
    memory_map_set_ram(&g_kernel_pagetable->memory_map, phy_ram_base,
                       phys_to_virt(phy_ram_base), phy_ram_size);

    // copy known memory regions from early memory map:
    memory_map_copy_from_early_memory_map(&g_kernel_pagetable->memory_map,
                                          &g_early_memory_map);

    memory_map_add_region_and_split(&g_kernel_pagetable->memory_map,
                                    virt_to_phys((size_t)__start_trampoline),
                                    (size_t)__start_trampoline, PAGE_SIZE,
                                    MM_REGION_TRAMPOLINE);

    // get initrd / ramdisk if present from the boot loader provided DTB!
    size_t initrd_base;
    size_t initrd_size;
    dtb_get_initrd(g_system.boot_dtb, &initrd_base, &initrd_size);
    if (initrd_base != 0 && initrd_size != 0)
    {
        memory_map_add_region_and_split(&g_kernel_pagetable->memory_map,
                                        initrd_base, phys_to_virt(initrd_base),
                                        PAGE_ROUND_UP(initrd_size),
                                        MM_REGION_INITRD);
    }

    kvm_apply_kernel_mapping(g_kernel_pagetable);

    // make all additional memory available for kmalloc()
    kalloc_init_memory(&g_kernel_pagetable->memory_map, MM_REGION_USABLE_RAM);

    if (memory_map_has_late_ram(&g_kernel_pagetable->memory_map))
    {
        memory_map_enable_late_ram(&g_kernel_pagetable->memory_map);
        kvm_apply_kernel_mapping(g_kernel_pagetable);
        kalloc_init_memory(&g_kernel_pagetable->memory_map, MM_REGION_LATE_RAM);
    }
}

void init_filesystem()
{
    // init filesystem:
    printk("init filesystem...\n");
    bio_init();  // buffer cache
    init_virtual_file_system();
    file_init();  // file table

    struct Devices_List *dev_list = get_devices_list();

    // find the device with the root file system:
    struct Found_Device *device_of_root_fs = NULL;
    struct Found_Device *ramdisk_dev =
        dev_list_get_first_device(dev_list, "ramdisk");
    struct Found_Device *disk_dev =
        dev_list_get_first_device(dev_list, "virtio,mmio");
    if (ramdisk_dev != NULL)
    {
        device_of_root_fs = ramdisk_dev;
    }
    else if (disk_dev != NULL)
    {
        device_of_root_fs = disk_dev;
    }
    else
    {
        panic("NO ROOT FILESYSTEM FOUND");
    }

    // store the device number of root:
    ROOT_DEVICE_NUMBER = device_of_root_fs->dev_num;
    printk("found root file system on device: %s (%d,%d)\n",
           device_of_root_fs->driver->dtb_name, MAJOR(ROOT_DEVICE_NUMBER),
           MINOR(ROOT_DEVICE_NUMBER));
}

void main(const void *dtb, bool is_boot_hart)
{
    cpu_set_boot_state();
    // define what interrupts should arrive, DOES NOT enable interrupts
    cpu_set_interrupt_mask();

    // install kernel trap vector before any other init to be able to catch and
    // print e.g. page faults
    set_supervisor_trap_vector();

    if (dtb == NULL)
    {
        dtb = g_system.dtb;  // for secondary cores
    }
    if ((dtb == NULL) || (fdt_magic(dtb) != FDT_MAGIC))
    {
        panic("No valid device tree found");
    }

    size_t cpu_id = smp_processor_id();
    if (is_boot_hart)
    {
        g_boot_hart = smp_processor_id();
        printk_init();  // printk might not print until a console driver is
                        // loaded!

        printk("\n");
        printk("VIMIX OS " __ARCH_bits_string " bit (" ARCH_NAME_STRING
               ") kernel version " str_from_define(GIT_HASH) " is booting\n");

        dtb = system_init_from_dtb(dtb);
        dtb_get_cpu_features(dtb, cpu_id, &g_cpus[cpu_id].features);
        system_print_info();

        init_kobject_root();
        timer_init(dtb);

        // after this kmalloc() is allowed
        init_memory_management(dtb);

        // after early device init printk() should definitely work
        init_devices(dtb);

        init_filesystem();

        init_userspace();  // including first user process

        ipi_init();
        g_kernel_init_status = KERNEL_INIT_FULLY_BOOTED;
        atomic_thread_fence(memory_order_seq_cst);
        system_boot_other_cpus(dtb);
    }
    else
    {
        dtb_get_cpu_features(dtb, cpu_id, &g_cpus[cpu_id].features);
    }
    ipi_init_per_cpu();

    timer_init_per_cpu();

    // init the interrupt controller that was found earlier
    g_int_con.init_per_cpu();

    g_cpus[cpu_id].state = CPU_STARTED;
    atomic_thread_fence(memory_order_seq_cst);

    printk("CPU %zd entering scheduler %s\n", cpu_id,
           (g_boot_hart == cpu_id ? "(boot CPU)" : ""));

    scheduler();
}
