/* SPDX-License-Identifier: MIT */

#include <drivers/device.h>
#include <init/dtb.h>
#include <kernel/major.h>
#include <kernel/string.h>
#include <lib/minmax.h>
#include <libfdt.h>
#include <mm/kalloc.h>

#if defined(__CONFIG_RAMDISK_EMBEDDED)
#include <ramdisk_fs.h>
#endif

struct Found_Device *found_device_alloc_init(
    struct Driver *driver, struct Device_Init_Parameters init_parameters)
{
    struct Found_Device *dev =
        kmalloc(sizeof(struct Found_Device), ALLOC_FLAG_ZERO_MEMORY);
    if (dev == NULL)
    {
        return NULL;
    }
    list_init(&dev->list);
    dev->driver = driver;
    dev->init_parameters = init_parameters;
    dev->dev_num = INVALID_DEVICE;
    return dev;
}

struct Devices_List g_devices_list;

void dev_list_init() { list_init(&g_devices_list.devices); }

struct Devices_List *get_devices_list()
{
    static bool is_initialized = false;
    if (!is_initialized)
    {
        dev_list_init();
        is_initialized = true;
    }
    return &g_devices_list;
}

void dev_list_add_virtual_devices(struct Devices_List *dev_list)
{
    struct Device_Init_Parameters defaults;
    clear_init_parameters(&defaults);

    for_each_driver(virt_driver)
    {
        if (virt_driver->type == VIRTUAL)
        {
            dev_list_add_with_parameters(dev_list, virt_driver, defaults);
        }
    }
}

// init_device() calls init_device_by_phandle() for dependent devices,
// this is seens as a potential infinite recursion by GCC analize which is a
// false positive as long as there is no loop in the devices dependencies.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-infinite-recursion"

/// @brief Init one individual device.
/// @param dev_list devices list (to also find dependencies)
/// @param dev pointer to the device info to initialize
/// @return valid device number if the device was initialized
dev_t init_device(struct Devices_List *dev_list, struct Found_Device *dev);

dev_t init_device_by_phandle(struct Devices_List *dev_list, int phandle)
{
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *dev = found_device_from_devices_list(pos);
        if (dev->init_parameters.phandle == phandle)
        {
            return init_device(dev_list, dev);
        }
    }
    return INVALID_DEVICE;
}

dev_t init_device(struct Devices_List *dev_list, struct Found_Device *dev)
{
    // already initialized:
    if (dev->dev_num != INVALID_DEVICE) return dev->dev_num;

    // found, double check pointer
    if (dev->driver->init_func != NULL)
    {
        // init required other drivers first:
        int32_t parent_int_ctl = dev->init_parameters.interrupt_parent_phandle;
        if (parent_int_ctl != 0)
        {
            // make sure the interrupt controller is initialized:
            init_device_by_phandle(dev_list, parent_int_ctl);
        }
        // init clocks:
        for (size_t i = 0; i < DEVICE_MAX_CLOCKS; ++i)
        {
            uint32_t clock_phandle = dev->init_parameters.clock_phandles[i];
            if (clock_phandle != 0)
            {
                init_device_by_phandle(dev_list, clock_phandle);
            }
        }

        dev_t dev_num = dev->driver->init_func(&(dev->init_parameters),
                                               dev->driver->dtb_name);
        dev->dev_num = dev_num;
        if (dev_num == 0)
        {
            // silently fail, some devices are optional like qemus virtio
            // devices
            return INVALID_DEVICE;
        }
        else
        {
            printk("init device %s... OK (%d,%d)\n", dev->driver->dtb_name,
                   MAJOR(dev_num), MINOR(dev_num));
            return dev_num;
        }
    }
    return INVALID_DEVICE;
}

dev_t init_device_by_name(struct Devices_List *dev_list, const char *dtb_name)
{
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *dev = found_device_from_devices_list(pos);
        if (strcmp(dev->driver->dtb_name, dtb_name) == 0)
        {
            return init_device(dev_list, dev);
        }
    }
    return INVALID_DEVICE;
}
#pragma GCC diagnostic pop

void clear_init_parameters(struct Device_Init_Parameters *param)
{
    param->interrupt = INVALID_IRQ_NUMBER;
    memset(param->mem, 0,
           sizeof(struct Memory_Mapped_Registers) * DEVICE_MAX_MEM_MAPS);
    param->mmu_map_memory = false;
    param->reg_io_width = 1;
    param->reg_shift = 0;
    param->dtb = NULL;
    param->phandle = 0;
    param->interrupt_parent_phandle = 0;
    memset(param->clock_phandles, 0, sizeof(uint32_t) * DEVICE_MAX_CLOCKS);
}

struct Found_Device *dev_list_get_first_device(struct Devices_List *dev_list,
                                               const char *name)
{
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *dev = found_device_from_devices_list(pos);
        if ((dev->dev_num != INVALID_DEVICE) &&
            (strcmp(dev->driver->dtb_name, name) == 0))
        {
            return dev;
        }
    }
    return NULL;
}

void dev_list_init_all_devices(struct Devices_List *dev_list)
{
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *dev = found_device_from_devices_list(pos);
        init_device(dev_list, dev);
    }
}

ssize_t dev_list_add_with_parameters(
    struct Devices_List *dev_list, struct Driver *driver,
    struct Device_Init_Parameters init_parameters)
{
    struct Found_Device *new_dev =
        found_device_alloc_init(driver, init_parameters);
    if (new_dev == NULL)
    {
        printk(
            "dev_list_add_with_parameters: out of memory adding device "
            "%s\n",
            driver->dtb_name);
        return -1;
    }

    bool added = false;
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *list_dev = found_device_from_devices_list(pos);
        if (new_dev->init_parameters.mem[0].start_pa <
            list_dev->init_parameters.mem[0].start_pa)
        {
            list_add(&new_dev->list, pos->prev);
            added = true;
            break;
        }
    }
    if (!added)
    {
        list_add_tail(&new_dev->list, &dev_list->devices);
    }

    return 0;
}

ssize_t dev_list_add_from_dtb(struct Devices_List *dev_list, void *dtb,
                              const char *device_name, int device_offset,
                              struct Driver *driver)
{
    // reset/default values for init parameters:
    struct Device_Init_Parameters params;
    clear_init_parameters(&params);
    params.dtb = dtb;
    params.dev_offset = device_offset;

    // query memory mapped registers
    dtb_get_regs(dtb, device_offset, &params);

    // get own phandle, set to 0 if there is none defined:
    params.phandle = fdt_get_phandle(dtb, device_offset);

    params.interrupt_parent_phandle =
        dtb_getprop32_with_fallback(dtb, device_offset, "interrupt-parent", 0);

    // 0 = no parameter, one int per clock; 1 = + parameter = 2 ints
    size_t clock_cells =
        dtb_getprop32_with_fallback(dtb, device_offset, "#clock-cells", 1);
    clock_cells++;  // actual int count
    int clocks_len;
    const uint32_t *clocks =
        fdt_getprop(dtb, device_offset, "clocks", &clocks_len);
    clocks_len /= sizeof(uint32_t);  // in cells
    if (clocks != NULL)
    {
        for (size_t i = 0;
             (i < clocks_len / clock_cells) && (i < DEVICE_MAX_CLOCKS); i++)
        {
            uint32_t phandle_clock = fdt32_to_cpu(clocks[i * clock_cells]);
            params.clock_phandles[i] = phandle_clock;
        }
    }

    // assumes a single interrupt, when parsing a list also parse
    // #interrupt-cells
    params.interrupt = dtb_getprop32_with_fallback(
        dtb, device_offset, "interrupts", params.interrupt);

    return dev_list_add_with_parameters(dev_list, driver, params);
}

void debug_print_found_device(struct Found_Device *dev)
{
    printk("Device %s ", dev->driver->dtb_name);
    if (dev->init_parameters.mem[0].size != 0)
    {
        printk("at 0x%zx size: 0x%zx ", dev->init_parameters.mem[0].start_pa,
               dev->init_parameters.mem[0].size);
        printk("mapped to va: 0x%zx ", dev->init_parameters.mem[0].start_va);
        printk("reg-width: %d, reg-shift: %d ",
               dev->init_parameters.reg_io_width,
               dev->init_parameters.reg_shift);
    }
    if (dev->init_parameters.interrupt != INVALID_IRQ_NUMBER)
    {
        printk("interrupt: %d ", dev->init_parameters.interrupt);
    }
    if (dev->init_parameters.phandle)
    {
        printk("phandle: %d ", dev->init_parameters.phandle);
    }
    if (dev->init_parameters.interrupt_parent_phandle)
    {
        printk("int-parent phandle: %d ",
               dev->init_parameters.interrupt_parent_phandle);
    }

    for (size_t c = 0;
         dev->init_parameters.clock_phandles[c] != 0 && c < DEVICE_MAX_CLOCKS;
         ++c)
    {
        printk("clock: %d ", dev->init_parameters.clock_phandles[c]);
    }
    printk("\n");
}

void debug_dev_list_print(struct Devices_List *dev_list)
{
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *dev = found_device_from_devices_list(pos);
        debug_print_found_device(dev);
    }
}
