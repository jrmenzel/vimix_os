/* SPDX-License-Identifier: MIT */

#include <drivers/device.h>
#include <drivers/devices_list.h>
#include <init/dtb.h>
#include <init/main.h>
#include <libfdt.h>

bool is_compatible_device(const char *dtb_dev, const char *dev)
{
    size_t size_of_dev_str = strlen(dev) + 1;

    // dtb_dev is a list of nul-terminated strings of compatible devices
    while (true)
    {
        if (strncmp(dtb_dev, dev, size_of_dev_str) == 0)
        {
            return true;
        }
        dtb_dev += strlen(dtb_dev) + 1;
        if (dtb_dev[0] == 0) break;
    }
    return false;
}

ssize_t dtb_add_driver_if_compatible(void *dtb, const char *device_name,
                                     int device_offset,
                                     struct Device_Driver *driver,
                                     struct Devices_List *dev_list)
{
    for (; driver->dtb_name != NULL; driver++)
    {
        // find a compatible driver from the list
        if (is_compatible_device(device_name, driver->dtb_name))
        {
            return dev_list_add_from_dtb(dev_list, dtb, device_name,
                                         device_offset, driver);
        }
    }
    return -1;
}

void dtb_add_devices_to_dev_list(void *dtb, struct Device_Driver *driver_list,
                                 struct Devices_List *dev_list)
{
    if (dtb == NULL)
    {
        return;
    }

    int off = 0;
    int depth = 0;
    while ((off = fdt_next_node(dtb, off, &depth)) >= 0)
    {
        const char *value = fdt_getprop(dtb, off, "compatible", NULL);
        if (value == NULL) continue;

        dtb_add_driver_if_compatible(dtb, value, off, driver_list, dev_list);
    }
}

void dtb_get_initrd(void *dtb, struct Minimal_Memory_Map *memory_map)
{
    size_t initrd_begin = 0;
    size_t initrd_end = 0;
    int offset = fdt_path_offset(dtb, "/chosen");
    if (offset >= 0)
    {
        // initrd-start / initrd-end can be stored as 64 or 32 bit (even on a 64
        // bit system). Assume both have the same size:
        int lenp = 0;
        const void *startp =
            fdt_getprop(dtb, offset, "linux,initrd-start", &lenp);
        const void *endp = fdt_getprop(dtb, offset, "linux,initrd-end", &lenp);

        if (startp && endp && lenp == 4)
        {
            // 32 bit values
            initrd_begin = (size_t)fdt32_to_cpu(*(const uint32_t *)startp);
            initrd_end = (size_t)fdt32_to_cpu(*(const uint32_t *)endp);
        }
        else if (startp && endp && lenp == 8)
        {
            // 64 bit values
            initrd_begin = (size_t)fdt64_to_cpu(*(const uint64_t *)startp);
            initrd_end = (size_t)fdt64_to_cpu(*(const uint64_t *)endp);
        }
    }
    memory_map->initrd_begin = initrd_begin;
    memory_map->initrd_end = initrd_end;
}

#ifndef MEMORY_SIZE
#error "define a fallback memory size"
#endif

void dtb_get_memory(void *dtb, struct Minimal_Memory_Map *memory_map)
{
    // fallback values
    memory_map->ram_start = 0x80000000L;
    memory_map->kernel_start = (size_t)start_of_kernel;
    memory_map->kernel_end = (size_t)end_of_kernel;
    memory_map->ram_end = (memory_map->ram_start + MEMORY_SIZE * 1024 * 1024);
    memory_map->dtb_file_start = 0;
    memory_map->dtb_file_end = 0;

    if (dtb == NULL)
    {
        return;
    }
    memory_map->dtb_file_start = (size_t)dtb;
    memory_map->dtb_file_end = (size_t)dtb + fdt_totalsize(dtb);

    int offset = fdt_path_offset(dtb, "/memory");
    if (offset < 0)
    {
        printk("dtb error: %s\n", (char *)fdt_strerror(offset));
        return;
    }
    int error;
    const uint64_t *value = fdt_getprop(dtb, offset, "reg", &error);
    if (value == NULL)
    {
        printk("dtb error: %s\n", (char *)fdt_strerror(error));
        return;
    }
    uint64_t base = fdt64_to_cpu(value[0]);
    uint64_t size = fdt64_to_cpu(value[1]);

    memory_map->ram_start = base;
    memory_map->ram_end = base + size;

    dtb_get_initrd(dtb, memory_map);
}

// note: gets called too early for printk...
uint64_t dtb_get_timebase(void *dtb)
{
    if (dtb == NULL)
    {
        return 0;
    }

    int offset = fdt_path_offset(dtb, "/cpus");
    if (offset < 0)
    {
        // printk("dtb error: %s\n", (char *)fdt_strerror(offset));
        return 0;
    }
    int error;
    const uint32_t *value =
        fdt_getprop(dtb, offset, "timebase-frequency", &error);
    if (value == NULL)
    {
        // printk("dtb error: %s\n", (char *)fdt_strerror(error));
        return 0;
    }
    uint64_t timebase = fdt32_to_cpu(value[0]);

    return timebase;
}

ssize_t dtb_add_boot_console_to_dev_list(void *dtb,
                                         struct Devices_List *dev_list)
{
    // find /chosen/stdout-path
    int offset = fdt_path_offset(dtb, "/chosen");
    if (offset < 0) return offset;  // contains a negative error code

    int lenp = 0;  // string length incl. 0-terminator
    const void *console = fdt_getprop(dtb, offset, "stdout-path", &lenp);
    if (console == NULL) return -1;

#define MAX_NAME_LEN 64
    char name[MAX_NAME_LEN];
    strncpy(name, console, MAX_NAME_LEN);

    // remove the baud rate if present:
    // e.g. "/soc/serial@10000000:115200" -> "/soc/serial@10000000"
    for (char *pos = name; *pos != 0; pos++)
    {
        if (*pos == ':')
        {
            *pos = 0;
            break;
        }
    }

    int console_offset = fdt_path_offset(dtb, name);
    if (console_offset < 0)
        return console_offset;  // contains a negative error code

    // see what it is compatible with...
    const char *value = fdt_getprop(dtb, console_offset, "compatible", NULL);
    if (value == NULL) return -1;

    return dtb_add_driver_if_compatible(dtb, value, console_offset,
                                        get_console_drivers(), dev_list);
}

int32_t dtb_getprop32_with_fallback(const void *dtb, int node_offset,
                                    const char *name, int32_t fallback)
{
    const fdt32_t *intp = (fdt32_t *)fdt_getprop(dtb, node_offset, name, NULL);
    if (intp != NULL)
    {
        int32_t value = fdt32_to_cpu(*intp);
        return value;
    }

    return fallback;
}