/* SPDX-License-Identifier: MIT */

#if defined(_ARCH_riscv)
#include <arch/riscv/clint.h>
#include <arch/riscv/plic.h>
#endif  // _ARCH_riscv

#include <drivers/console.h>
#include <drivers/dev_null.h>
#include <drivers/dev_random.h>
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
#include <lib/minmax.h>
#include <libfdt.h>

#if defined(_PLATFORM_VISIONFIVE2)
#include <drivers/jh7110_clk.h>
#include <drivers/jh7110_temp.h>
#endif

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
bool g_devices_list_is_initialized = false;

// clang-format off
struct Device_Driver g_console_drivers[] = {{"ns16550a", console_init, EARLY_CONSOLE},
                                            {"snps,dw-apb-uart", console_init, EARLY_CONSOLE},
                                            {"ucb,htif0", console_init, EARLY_CONSOLE},
                                            {NULL, NULL, 0}};

// don't need any specific hardware
struct Device_Driver g_virtual_drivers[] = {{"/dev/null", dev_null_init, REGULAR_DEVICE},
                                            {"/dev/zero", dev_zero_init, REGULAR_DEVICE},
                                            {"/dev/random", dev_random_init, REGULAR_DEVICE},
                                            {NULL, NULL, 0}};

struct Device_Driver g_generell_drivers[] = {{"virtio,mmio", virtio_disk_init, REGULAR_DEVICE},
                                             {"google,goldfish-rtc", rtc_init, CLOCK_DRIVER}, // init before /dev/random if present
                                             {"syscon", syscon_init, REGULAR_DEVICE},
                                             {"ucb,htif0", htif_init, REGULAR_DEVICE},
#if defined(_ARCH_riscv)
                                             {"riscv,plic0", plic_init, INTERRUPT_CONTROLLER},
#if defined(__BOOT_M_MODE)
                                             {"riscv,clint0", clint_init, INTERRUPT_CONTROLLER},
#endif
#if defined(_PLATFORM_VISIONFIVE2)
                                             {"starfive,jh7110-clkgen", jh7110_clk_init, CLOCK_DRIVER},
//                                           {"starfive,jh7110-temp", jh7110_temp_init, REGULAR_DEVICE}, // see below: not in device tree
#endif // _PLATFORM_VISIONFIVE2
#endif // _ARCH_riscv
                                             {NULL, NULL, 0}};

// not found in the device tree, so added explicitly
struct Device_Driver g_ramdisk_driver = {"ramdisk", ramdisk_init, REGULAR_DEVICE};
#if defined(_PLATFORM_VISIONFIVE2)
struct Device_Driver jh7110_temp = {"starfive,jh7110-temp", jh7110_temp_init, REGULAR_DEVICE};
#endif
// clang-format on

struct Devices_List *get_devices_list()
{
    if (!g_devices_list_is_initialized)
    {
        // add devices that are always present (vs. found in the device tree)
        struct Device_Driver *driver = g_virtual_drivers;
        struct Device_Init_Parameters defaults;
        clear_init_parameters(&defaults);
        while (driver->dtb_name != NULL)
        {
            dev_list_add_with_parameters(&g_devices_list, driver, defaults);
            driver++;
        }

#if defined(_PLATFORM_VISIONFIVE2)
        // This should be an entry in g_generell_drivers but the temp sensor is
        // not in the device tree file found on the visionfive2 board.
        struct Device_Init_Parameters jh7110_parameters;
        clear_init_parameters(&jh7110_parameters);
        jh7110_parameters.mem[0].start = 0x120e0000;
        jh7110_parameters.mem[0].size = 0x10000;
        jh7110_parameters.mmu_map_memory = true;
        jh7110_parameters.reg_shift = 0;
        jh7110_parameters.reg_io_width = 1;
        jh7110_parameters.interrupt = 81;
        dev_list_add_with_parameters(&g_devices_list, &jh7110_temp,
                                     jh7110_parameters);
#endif

        g_devices_list_is_initialized = true;
    }
    return &g_devices_list;
}

struct Device_Driver *get_console_drivers() { return g_console_drivers; }
struct Device_Driver *get_generell_drivers() { return g_generell_drivers; }

void init_device(struct Found_Device *dev)
{
    // found, double check pointer, not already initialized
    if (dev->driver->init_func != NULL && dev->dev_num == 0)
    {
        // NOTE: the first device to init is the console for boot
        // messages. No printk before the init function in case this is the
        // first console
        dev_t dev_num = dev->driver->init_func(&(dev->init_parameters),
                                               dev->driver->dtb_name);
        dev->dev_num = dev_num;
        printk("init device %s... ", dev->driver->dtb_name);
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
    memset(param->mem, 0,
           sizeof(struct Memory_Mapped_Registers) * DEVICE_MAX_MEM_MAPS);
    param->mmu_map_memory = false;
    param->reg_io_width = 1;
    param->reg_shift = 0;
    param->dtb = NULL;
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
            (strcmp(dev->driver->dtb_name, name) == 0))
        {
            if (dev->init_parameters.mem[0].start < addr_fist)
            {
                index_first = i;
                addr_fist = dev->init_parameters.mem[0].start;
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
        if (dev->driver->dtb_name && strcmp(dev->driver->dtb_name, name) == 0)
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

void dev_list_init_all_devices_of_init_order(struct Devices_List *dev_list,
                                             enum Init_Order init_order)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        if (dev_list->dev[i].driver->init_order == init_order)
        {
            init_device(&(dev_list->dev[i]));
        }
    }
}

void dev_list_init_all_devices(struct Devices_List *dev_list)
{
    // EARLY_CONSOLE is already done here
    dev_list_init_all_devices_of_init_order(dev_list, CLOCK_DRIVER);
    dev_list_init_all_devices_of_init_order(dev_list, REGULAR_DEVICE);
    dev_list_init_all_devices_of_init_order(dev_list, INTERRUPT_CONTROLLER);
}

ssize_t dev_list_add_with_parameters(
    struct Devices_List *dev_list, struct Device_Driver *driver,
    struct Device_Init_Parameters init_parameters)
{
    ssize_t idx = 0;
    struct Found_Device *dev = dev_list_get_free_device(dev_list, &idx);
    if (dev == NULL) return -1;

    dev->init_parameters = init_parameters;
    dev->driver = driver;
    dev->dev_num = 0;  // to be set in init

    return idx;
}

ssize_t dev_list_add_from_dtb(struct Devices_List *dev_list, void *dtb,
                              const char *device_name, int device_offset,
                              struct Device_Driver *driver)
{
    // reset/default values for init parameters:
    struct Device_Init_Parameters params;
    clear_init_parameters(&params);
    params.dtb = dtb;
    params.dev_offset = device_offset;

    int len;
    const dtb_aligned_uint64_t *regs =
        fdt_getprop(dtb, device_offset, "reg", &len);
    int len_names;
    const char *reg_names =
        fdt_getprop(dtb, device_offset, "reg-names", &len_names);
    if (regs != NULL)
    {
        // len is in bytes, reg_size is the count of ints per register map:
        size_t reg_size = (2 + 2) * sizeof(uint32_t);
        size_t regs_to_read = min(len / reg_size, DEVICE_MAX_MEM_MAPS);
        size_t reg_idx_start = 0;
        size_t reg_idx_size = 1;
        for (size_t i = 0; i < regs_to_read; ++i)
        {
            params.mem[i].start = fdt64_to_cpu(regs[reg_idx_start]);
            params.mem[i].size = fdt64_to_cpu(regs[reg_idx_size]);
            reg_idx_start += reg_size / sizeof(uint64_t);
            reg_idx_size += reg_size / sizeof(uint64_t);

            // store name if available:
            if (reg_names && len_names > 0)
            {
                params.mem[i].name = reg_names;

                // reg_names is a list of len_names 0-terminated strings,
                // find the next one:
                do {
                    reg_names++;
                    len_names--;
                } while (*reg_names != 0);
                reg_names++;
                len_names--;
            }
        }

        params.mmu_map_memory = true;

        // might also have reg-shift
        params.reg_io_width = dtb_getprop32_with_fallback(
            dtb, device_offset, "reg-io-width", params.reg_io_width);

        params.reg_shift = dtb_getprop32_with_fallback(
            dtb, device_offset, "reg-shift", params.reg_shift);
    }
    params.interrupt = dtb_getprop32_with_fallback(
        dtb, device_offset, "interrupts", params.interrupt);

    return dev_list_add_with_parameters(dev_list, driver, params);
}

void dev_list_sort(struct Devices_List *dev_list, const char *name)
{
    // can be more than MAX_MINOR_DEVICES, which is the limit for
    // initialized devices
    const size_t MAX_DEVICES_PER_TYPE = 32;
    ssize_t index[MAX_DEVICES_PER_TYPE];
    for (size_t i = 0; i < MAX_DEVICES_PER_TYPE; ++i)
    {
        index[i] = -1;
    }

    // find devices of the given name
    size_t next_index = 0;
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &(dev_list->dev[i]);
        if (strcmp(dev->driver->dtb_name, name) == 0)
        {
            index[next_index++] = i;
        }
    }

    // primitive bubble sort
    bool swapped = false;
    do {
        swapped = false;
        size_t max_addr = 0;
        size_t max_addr_index = 0;
        for (size_t i = 0; i < next_index; ++i)
        {
            ssize_t idx = index[i];
            size_t addr = dev_list->dev[idx].init_parameters.mem[0].start;
            if (addr < max_addr)
            {
                // swap idx and max_addr_index
                struct Found_Device tmp = dev_list->dev[idx];
                dev_list->dev[idx] = dev_list->dev[max_addr_index];
                dev_list->dev[max_addr_index] = tmp;
                swapped = true;
            }
            else
            {
                max_addr = addr;
                max_addr_index = idx;
            }
        }
    } while (swapped);
}

void debug_dev_list_print(struct Devices_List *dev_list)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &dev_list->dev[i];
        printk("Found device %s ", dev->driver->dtb_name);
        if (dev->init_parameters.mem[0].size != 0)
        {
            printk("at 0x%zx size: 0x%zx ", dev->init_parameters.mem[0].start,
                   dev->init_parameters.mem[0].size);
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
