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

void dtb_get_devices(void *dtb, struct Devices_List *dev_list)
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
        for (size_t i = 0; i < dev_list->dev_array_length; ++i)
        {
            struct Supported_Device *dev = &(dev_list->dev[i]);
            if (value && (dev->found == false) &&
                is_compatible_device(value, dev->dtb_name))
            {
                uint64_t base_addr = 0;
                uint64_t size = 0;

                const uint64_t *regs = fdt_getprop(dtb, off, "reg", NULL);
                if (regs != NULL)
                {
                    base_addr = fdt64_to_cpu(regs[0]);
                    size = fdt64_to_cpu(regs[1]);
                }

                const fdt32_t *intp =
                    (fdt32_t *)fdt_getprop(dtb, off, "interrupts", NULL);

                if (intp)
                {
                    int32_t interrupt = fdt32_to_cpu(*intp);
                    dev->mapping.interrupt = interrupt;
                }
                else
                {
                    dev->mapping.interrupt = INVALID_IRQ_NUMBER;
                }

                dev->mapping.mem_start = base_addr;
                dev->mapping.mem_size = size;
                dev->found = true;

                // break loop looking for a supported device
                break;
            }
        }
    }
}

void dtb_get_initrd(void *dtb, struct Minimal_Memory_Map *memory_map)
{
    size_t initrd_begin = 0;
    size_t initrd_end = 0;
    int offset = fdt_path_offset(dtb, "/chosen");
    if (offset)
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
