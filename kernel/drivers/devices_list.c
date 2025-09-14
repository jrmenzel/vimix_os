/* SPDX-License-Identifier: MIT */

#if defined(__ARCH_riscv)
#include <arch/riscv/plic.h>
#include <drivers/jh7110_syscrg.h>
#include <drivers/jh7110_temp.h>
#endif  // __ARCH_riscv

#include <drivers/console.h>
#include <drivers/dev_null.h>
#include <drivers/dev_random.h>
#include <drivers/dev_zero.h>
#include <drivers/devices_list.h>
#include <drivers/htif.h>
#include <drivers/ramdisk.h>
#include <drivers/rtc.h>
#include <drivers/syscon.h>
#include <drivers/uart16550.h>
#include <drivers/virtio_disk.h>
#include <init/dtb.h>
#include <kernel/major.h>
#include <kernel/string.h>
#include <lib/minmax.h>
#include <libfdt.h>

#if defined(__CONFIG_RAMDISK_EMBEDDED)
#include <ramdisk_fs.h>
#endif

#define MAX_DEV_LIST_LENGTH 32

// The init_parameters will be set from the device tree if a device was found.
//   found should be false for everything intended to be read from the dtb
//   found can be true if a device should always be initialized with the
//   provided values / init function.
struct Found_Device g_found_devices[MAX_DEV_LIST_LENGTH] = {0};

struct Devices_List g_devices_list = {g_found_devices, 0};
bool g_devices_list_is_initialized = false;

// clang-format off
struct Device_Driver g_virtual_drivers[] = {{"/dev/null", dev_null_init},
                                            {"/dev/zero", dev_zero_init},
                                            {"/dev/random", dev_random_init},
                                            {NULL, NULL }};

struct Device_Driver g_generell_drivers[] = {{"ns16550a", uart_init},
                                             {"snps,dw-apb-uart", uart_init},
                                             {"ucb,htif0", htif_init},
                                             {"virtio,mmio", virtio_disk_init},
                                             {"google,goldfish-rtc", rtc_init},
                                             {"syscon", syscon_init},

#if defined(__ARCH_riscv)
                                             {"riscv,plic0", plic_init},
                                             {"sifive,plic-1.0.0", plic_init},
                                             {"starfive,jh7110-syscrg", jh7110_syscrg_init},
                                             {"starfive,jh7110-temp", jh7110_temp_init},
#endif // __ARCH_riscv
                                             {NULL, NULL}};

// not found in the device tree, so added explicitly
struct Device_Driver g_ramdisk_driver = {"ramdisk", ramdisk_init};
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

        g_devices_list_is_initialized = true;
    }
    return &g_devices_list;
}

struct Device_Driver *get_generell_drivers() { return g_generell_drivers; }

// init_device() calls init_device_by_phandle() for dependent devices,
// this is seens as a potential infinite recursion by GCC analize which is a
// false positive as long as there is no loop in the devices dependencies.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-infinite-recursion"

dev_t init_device_by_phandle(struct Devices_List *dev_list, int phandle)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        if (dev_list->dev[i].init_parameters.phandle == phandle)
        {
            return init_device(dev_list, i);
        }
    }
    return INVALID_DEVICE;
}

dev_t init_device(struct Devices_List *dev_list, size_t index)
{
    struct Found_Device *dev = &(dev_list->dev[index]);

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

        printk("init device %s... ", dev->driver->dtb_name);
        dev_t dev_num = dev->driver->init_func(&(dev->init_parameters),
                                               dev->driver->dtb_name);
        dev->dev_num = dev_num;
        if (dev_num == 0)
        {
            printk("FAILED\n");
            return INVALID_DEVICE;
        }
        else
        {
            printk("OK (%d,%d)\n", MAJOR(dev_num), MINOR(dev_num));
            return dev_num;
        }
    }
    return INVALID_DEVICE;
}

dev_t init_device_by_name(struct Devices_List *dev_list, const char *dtb_name)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        if (strcmp(dev_list->dev[i].driver->dtb_name, dtb_name) == 0)
        {
            return init_device(dev_list, i);
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

void dev_list_init_all_devices(struct Devices_List *dev_list)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        init_device(dev_list, i);
    }
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
    dev->dev_num = INVALID_DEVICE;  // to be set in init

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

void dev_list_sort(struct Devices_List *dev_list, const char *name)
{
    // 32 should be enough
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

        for (size_t c = 0; dev->init_parameters.clock_phandles[c] != 0 &&
                           c < DEVICE_MAX_CLOCKS;
             ++c)
        {
            printk("clock: %d ", dev->init_parameters.clock_phandles[c]);
        }
        printk("\n");
    }
}
