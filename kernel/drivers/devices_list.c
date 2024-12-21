/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <arch/riscv/plic.h>
#include <drivers/console.h>
#include <drivers/dev_null.h>
#include <drivers/dev_zero.h>
#include <drivers/devices_list.h>
#include <drivers/htif.h>
#include <drivers/ramdisk.h>
#include <drivers/rtc.h>
#include <drivers/syscon.h>
#include <drivers/virtio_disk.h>
#include <init/dtb.h>
#include <kernel/major.h>
#include <kernel/string.h>
#include <libfdt.h>

#ifdef RAMDISK_EMBEDDED
#include <ramdisk_fs.h>
#endif

#define MAX_DEV_LIST_LENGTH 32

// The init_parameters will be set from the device tree if a device was found.
//   found should be false for everything intended to be read from the dtb
//   found can be true if a device should always be initialized with the
//   provided values / init function.
// Devices are initialized in array order, all devices with interrupts have to
// come before the CLINT.
struct Found_Device g_found_devices[MAX_DEV_LIST_LENGTH] = {0};

struct Devices_List g_devices_list = {g_found_devices, 0};

struct Devices_List *get_devices_list() { return &g_devices_list; }

// clang-format off
struct Device_Driver g_console_drivers[] = {{"ns16550a", console_init},
//                                          {"snps,dw-apb-uart", console_init},
                                            {"ucb,htif0", console_init},
                                            {NULL, NULL}};

struct Device_Driver g_generell_drivers[] = {{"virtio,mmio", virtio_disk_init},
                                             {"google,goldfish-rtc", rtc_init},
                                             {"syscon", syscon_init},
                                             {"ucb,htif0", htif_init},
                                             {"riscv,plic0", plic_init},
#if defined(__TIMER_SOURCE_CLINT)
                                             {"riscv,clint0", clint_init},
#endif
                                             {NULL, NULL}};
// clang-format on

struct Device_Driver *get_console_drivers() { return g_console_drivers; }
struct Device_Driver *get_generell_drivers() { return g_generell_drivers; }

void init_device(struct Found_Device *dev)
{
    // found, double check pointer, not already initialized
    if (dev->init_func != NULL && dev->dev_num == 0)
    {
        // NOTE: the first device to init is the console for boot
        // messages. No printk before the init function in case this is the
        // first console
        dev_t dev_num = dev->init_func(&(dev->init_parameters), dev->dtb_name);
        dev->dev_num = dev_num;
        printk("init device %s... ", dev->dtb_name);
        if (dev_num == 0)
        {
            printk("FAILED\n");
        }
        else
        {
            printk("OK (%d,%d)\n", MAJOR(dev_num), MINOR(dev_num));
        }
    }
}

void clear_init_parameters(struct Device_Init_Parameters *param)
{
    param->interrupt = INVALID_IRQ_NUMBER;
    param->mem_size = 0;
    param->mem_start = 0;
    param->mmu_map_memory = false;
    param->reg_io_width = 1;
    param->reg_shift = 0;
}

ssize_t dev_list_get_first_device_index(struct Devices_List *dev_list,
                                        const char *name)
{
    ssize_t index_first = -1;
    size_t addr_fist = (-1);  // max address
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &(dev_list->dev[i]);
        if ((dev->dev_num != INVALID_DEVICE) &&
            (strcmp(dev->dtb_name, name) == 0))
        {
            if (dev->init_parameters.mem_start < addr_fist)
            {
                index_first = i;
                addr_fist = dev->init_parameters.mem_start;
            }
        }
    }
    return index_first;
}

ssize_t dev_list_get_device_index(struct Devices_List *dev_list,
                                  const char *name)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &(dev_list->dev[i]);
        if (dev->dtb_name && strcmp(dev->dtb_name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

struct Found_Device *dev_list_get_free_device(struct Devices_List *dev_list,
                                              ssize_t *index)
{
    if (dev_list->dev_array_length == MAX_DEV_LIST_LENGTH)
    {
        printk("no device space left");
        return NULL;
    }
    struct Found_Device *dev = &dev_list->dev[dev_list->dev_array_length];
    if (index) *index = dev_list->dev_array_length;
    dev_list->dev_array_length++;
    return dev;
}

void dev_list_init_all_devices(struct Devices_List *dev_list)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        if (dev_list->dev[i].init_parameters.interrupt != INVALID_IRQ_NUMBER)
        {
            // step one: register all devices which require interrupts
            init_device(&(dev_list->dev[i]));
        }
    }
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        if (dev_list->dev[i].init_parameters.interrupt == INVALID_IRQ_NUMBER)
        {
            // step two: all remaining devices.
            // One of which will handle interrupts, so it needs to find the
            // complete list of required interrupts.
            init_device(&(dev_list->dev[i]));
        }
    }
}

ssize_t dev_list_add(struct Devices_List *dev_list, const char *name,
                     init_func_p init_func)
{
    struct Device_Init_Parameters defaults;
    clear_init_parameters(&defaults);
    return dev_list_add_with_parameters(dev_list, name, init_func, defaults);
}

ssize_t dev_list_add_with_parameters(
    struct Devices_List *dev_list, const char *name, init_func_p init_func,
    struct Device_Init_Parameters init_parameters)
{
    ssize_t idx = 0;
    struct Found_Device *dev = dev_list_get_free_device(dev_list, &idx);
    if (dev == NULL) return -1;

    dev->init_parameters = init_parameters;
    dev->dtb_name = name;
    dev->dev_num = 0;  // to be set in init
    dev->init_func = init_func;

    return idx;
}

ssize_t dev_list_add_from_dtb(struct Devices_List *dev_list, void *dtb,
                              const char *device_name, int device_offset,
                              struct Device_Driver *driver)
{
    // reset/default values for init parameters:
    struct Device_Init_Parameters params;
    clear_init_parameters(&params);

    const uint64_t *regs = fdt_getprop(dtb, device_offset, "reg", NULL);
    if (regs != NULL)
    {
        params.mem_start = fdt64_to_cpu(regs[0]);
        params.mem_size = fdt64_to_cpu(regs[1]);
        params.mmu_map_memory = true;

        // might also have reg-shift
        params.reg_io_width = dtb_getprop32_with_fallback(
            dtb, device_offset, "reg-io-width", params.reg_io_width);

        params.reg_shift = dtb_getprop32_with_fallback(
            dtb, device_offset, "reg-shift", params.reg_shift);
    }
    params.interrupt = dtb_getprop32_with_fallback(
        dtb, device_offset, "interrupts", params.interrupt);

    return dev_list_add_with_parameters(dev_list, driver->dtb_name,
                                        driver->init_func, params);
}

void debug_dev_list_print(struct Devices_List *dev_list)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &dev_list->dev[i];
        printk("Found device %s ", dev->dtb_name);
        if (dev->init_parameters.mem_size != 0)
        {
            printk("at 0x%zx size: 0x%zx ", dev->init_parameters.mem_start,
                   dev->init_parameters.mem_size);
            printk("reg-width: %d, reg-shift: %d ",
                   dev->init_parameters.reg_io_width,
                   dev->init_parameters.reg_shift);
        }
        if (dev->init_parameters.interrupt != -1)
        {
            printk("interrupt: %d", dev->init_parameters.interrupt);
        }
        printk("\n");
    }
}
