/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/platform.h>
#include <arch/timer.h>
#include <arch/trap.h>
#include <drivers/console.h>
#include <drivers/devices_list.h>
#include <drivers/ramdisk.h>
#include <drivers/virtio_disk.h>
#include <init/dtb.h>
#include <init/early_pgtable.h>
#include <init/main.h>
#include <init/start.h>
#include <kernel/bio.h>
#include <kernel/cpu.h>
#include <kernel/file.h>
#include <kernel/fs.h>
#include <kernel/kobject.h>
#include <kernel/kticks.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/scheduler.h>
#include <kernel/smp.h>
#include <mm/kalloc.h>
#include <mm/memlayout.h>
#include <mm/memory_map.h>
#include <mm/vm.h>

#if defined(__CONFIG_RAMDISK_EMBEDDED)
#include <ramdisk_fs.h>
#endif

// to get a string from the git version number define
#define str_from_define(s) str(s)
#define str(s) #s

#ifdef __ARCH_riscv
#define FEATURE_STRING "(RISC V)"
#endif

void print_timer_source(void *dtb)
{
    CPU_Features features = dtb_get_cpu_features(dtb, smp_processor_id());

    if (features & RV_EXT_SSTC)
    {
        printk("Timer source: sstc extension\n");
    }
    else
    {
        printk("Timer source: SBI\n");
    }
}

void add_ramdisks_to_dev_list(void *dtb, struct Devices_List *dev_list)
{
#if defined(__CONFIG_RAMDISK_EMBEDDED)
    struct Device_Init_Parameters init_params;
    clear_init_parameters(&init_params);
    init_params.mem[0].start = virt_to_phys((size_t)ramdisk_fs);
    init_params.mem[0].size = (size_t)ramdisk_fs_size;
    dev_list_add_with_parameters(dev_list, &g_ramdisk_driver, init_params);
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
        dev_list_add_with_parameters(dev_list, &g_ramdisk_driver, init_params);
    }
}

void init_devices(struct Devices_List *dev_list, void *dtb)
{
    printk("init devices list...\n");
    // Collect all found devices in this list for later init:
    dtb_add_devices_to_dev_list(dtb, get_generell_drivers(), dev_list);
    // add ramdisk if present:
    add_ramdisks_to_dev_list(dtb, dev_list);
    // sort for predictable device numbers:
    dev_list_sort(dev_list, "virtio,mmio");

    // init a way to print, starts uart:
    ssize_t con_idx = dtb_find_boot_console_in_dev_list(dtb, dev_list);

    // map devices
    memory_map_add_device_mmio(&g_kernel_pagetable->memory_map, dev_list);
    kvm_apply_mapping(g_kernel_pagetable);
    // now we are done with the page table: unlock
    spin_unlock(&g_kernel_pagetable->lock);

    if (con_idx >= 0)
    {
        printk("init console: %s\n", dev_list->dev[con_idx].driver->dtb_name);
        dev_t con_dev = console_init(&(dev_list->dev[con_idx].init_parameters),
                                     dev_list->dev[con_idx].driver->dtb_name);
        if (con_dev == INVALID_DEVICE)
        {
            panic("not a valid console");
        }
        printk_redirect_to_console();
    }
    else
    {
        panic("no console found");
    }

    printk("init remaining devices...\n");
    dev_list_init_all_devices(dev_list);
}

void init_memory_management(void *dtb)
{
    printk("init early memory management...\n");

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

    memory_map_add_region_and_split(
        &g_kernel_pagetable->memory_map, virt_to_phys((size_t)trampoline),
        TRAMPOLINE, PAGE_SIZE, MM_REGION_KERNEL_TEXT);

    // get initrd / ramdisk if present
    size_t initrd_base;
    size_t initrd_size;
    dtb_get_initrd(dtb, &initrd_base, &initrd_size);
    if (initrd_base != 0 && initrd_size != 0)
    {
        memory_map_add_region_and_split(&g_kernel_pagetable->memory_map,
                                        initrd_base, phys_to_virt(initrd_base),
                                        initrd_size, MM_REGION_INITRD);
    }

    kvm_apply_mapping(g_kernel_pagetable);

    // make all additional memory available for kmalloc()
    kalloc_init_memory(&g_kernel_pagetable->memory_map, MM_REGION_USABLE_RAM);
}

void init_filesystem(struct Devices_List *dev_list)
{
    // init filesystem:
    printk("init filesystem...\n");
    bio_init();  // buffer cache
    init_virtual_file_system();
    file_init();  // file table

    // find the device with the root file system:
    size_t device_of_root_fs = 0;
    ssize_t ramdisk_index =
        dev_list_get_first_device_index(dev_list, "ramdisk");
    ssize_t disk_index_0 =
        dev_list_get_first_device_index(dev_list, "virtio,mmio");
    if (ramdisk_index >= 0)
    {
        device_of_root_fs = ramdisk_index;
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
    printk("found root file system on device: %s (%d,%d)\n",
           dev_list->dev[device_of_root_fs].driver->dtb_name,
           MAJOR(ROOT_DEVICE_NUMBER), MINOR(ROOT_DEVICE_NUMBER));
}

void main(void *dtb, bool is_boot_hart)
{
    cpu_set_boot_state();
    // define what interrupts should arrive, DOES NOT enable interrupts
    cpu_set_interrupt_mask();

    // install kernel trap vector before any other init to be able to catch and
    // print e.g. page faults
    set_supervisor_trap_vector();

    if (fdt_magic(dtb) != FDT_MAGIC)
    {
        panic("No valid device tree found");
    }

    if (is_boot_hart)
    {
        g_boot_hart = smp_processor_id();
        printk_init();  // printk might not print until a console driver is
                        // loaded!

        printk("\n");
        printk("VIMIX OS " __ARCH_bits_string " bit " FEATURE_STRING
               " kernel version " str_from_define(GIT_HASH) " is booting\n");
        print_timer_source(dtb);

        init_kobject_root();
        kticks_init();
        init_platform();

        // after this kmalloc() is allowed
        init_memory_management(dtb);

        // after early device init printk() should definitely work
        struct Devices_List *dev_list = get_devices_list();
        init_devices(dev_list, dtb);

        init_filesystem(dev_list);

        init_userspace();  // including first user process

        // get the timebase frequency for timer_init():
        g_timebase_frequency = dtb_get_timebase(dtb);

        ipi_init();
        g_kernel_init_status = KERNEL_INIT_FULLY_BOOTED;
        atomic_thread_fence(memory_order_seq_cst);
        platform_boot_other_cpus(dtb);
    }

    size_t cpu_id = smp_processor_id();
    g_cpus[cpu_id].features = dtb_get_cpu_features(dtb, cpu_id);
    timer_init(dtb, g_cpus[cpu_id].features);
    init_interrupt_controller_per_hart();

    g_cpus[cpu_id].state = CPU_STARTED;

    printk("CPU %zd entering scheduler %s\n", cpu_id,
           (g_boot_hart == cpu_id ? "(boot CPU)" : ""));

    scheduler();
}
