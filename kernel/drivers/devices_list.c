/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <arch/riscv/plic.h>
#include <drivers/console.h>
#include <drivers/dev_null.h>
#include <drivers/dev_zero.h>
#include <drivers/devices_list.h>
#include <drivers/ramdisk.h>
#include <drivers/rtc.h>
#include <drivers/syscon.h>
#include <drivers/virtio_disk.h>
#include <kernel/major.h>
#include <kernel/string.h>

#ifdef RAMDISK_EMBEDDED
#include <ramdisk_fs.h>
#endif

// The mapping will be set from the device tree if a device was found.
//   found should be false for everything intended to be read from the dtb
//   found can be true if a device should always be initialized with the
//   provided values / init function.
// Devices are initialized in array order, all devices with interrupts have to
// come before the CLINT.
// clang-format off
struct Supported_Device g_supported_devices[] = {
//  name (searched in dtb) |found| init func  |dev_t|map memory| {mem map}
    // first one must be the boot console:
#ifdef _PLATFORM_VISIONFIVE2
    {"snps,dw-apb-uart",    true, console_init,  0, true,  {0x10000000L, PAGE_SIZE, 32}},
#else
    {"ns16550a",            false, console_init,  0, true,  {0x10000000L, PAGE_SIZE, 10}},
#endif
    {"ramdisk_initrd",     false, ramdisk_init,  0, false, {0,0,-1}},
#ifdef RAMDISK_EMBEDDED                                                   
    {"ramdisk_embedded",    true, ramdisk_init,  0, false, {(size_t)ramdisk_fs,  ramdisk_fs_size, -1}},
#endif                                                                    
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10001000L, PAGE_SIZE, 1}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10002000L, PAGE_SIZE, 2}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10003000L, PAGE_SIZE, 3}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10004000L, PAGE_SIZE, 4}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10005000L, PAGE_SIZE, 5}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10006000L, PAGE_SIZE, 6}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10007000L, PAGE_SIZE, 7}},
    {"virtio,mmio",         false, virtio_disk_init, 0, true,  {0x10008000L, PAGE_SIZE, 8}},
    {"google,goldfish-rtc", false, rtc_init,      0, true,  {0x101000L,   PAGE_SIZE, 11}},
#ifndef _PLATFORM_VISIONFIVE2
    // used for shutdown on qemu, but incompatible with visionfive2
    {"syscon",              false, syscon_init,   0, true,  {0x100000L,   PAGE_SIZE, -1}},
#endif
    {"riscv,plic0",         false, plic_init,     0, true,  {0xc000000L,  0x600000L, -1}},
#if defined(__TIMER_SOURCE_CLINT)
    {"riscv,clint0",        false, clint_init,    0, true,  {0x2000000L,  PAGE_SIZE, -1}},
#endif
    {"/dev/null",           true,  dev_null_init, 0, false, {0,0,-1}},
    {"/dev/zero",           true,  dev_zero_init, 0, false, {0,0,-1}}
};
// clang-format on

const size_t g_supported_devices_count =
    sizeof(g_supported_devices) / sizeof(g_supported_devices[0]);

struct Devices_List g_devices_list = {g_supported_devices,
                                      g_supported_devices_count};

struct Devices_List *get_devices_list() { return &g_devices_list; }

void print_found_devices()
{
    for (size_t i = 0; i < g_supported_devices_count; ++i)
    {
        struct Supported_Device *dev = &g_supported_devices[i];
        if (dev->found)
        {
            printk("Found device %s at 0x%zx size: 0x%zx", dev->dtb_name,
                   dev->mapping.mem_start, dev->mapping.mem_size);
            if (dev->mapping.interrupt != -1)
            {
                printk(" interrupt: %d", dev->mapping.interrupt);
            }
            printk("\n");
        }
    }
}

ssize_t get_first_virtio(struct Devices_List *dev_list)
{
    ssize_t index_first = -1;
    size_t addr_fist = (-1);
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Supported_Device *dev = &(dev_list->dev[i]);
        if (dev->found && (dev->dev_num != INVALID_DEVICE) &&
            (strcmp(dev->dtb_name, "virtio,mmio") == 0))
        {
            if (dev->mapping.mem_start < addr_fist)
            {
                index_first = i;
                addr_fist = dev->mapping.mem_start;
            }
        }
    }
    return index_first;
}

ssize_t get_device_index(struct Devices_List *dev_list, const char *name)
{
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Supported_Device *dev = &(dev_list->dev[i]);
        if (strcmp(dev->dtb_name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

void init_device(struct Supported_Device *dev)
{
    if (dev->found && dev->init_func != NULL)
    {
        // NOTE: the first device to init is the console for boot
        // messages. No printk before the init function in case this is the
        // first console
        dev_t dev_num = dev->init_func(&(dev->mapping));
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
