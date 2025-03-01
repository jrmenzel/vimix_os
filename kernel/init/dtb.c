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
    if (fdt_magic(dtb) != FDT_MAGIC)
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
            initrd_begin =
                (size_t)fdt64_to_cpu(*(const dtb_aligned_uint64_t *)startp);
            initrd_end =
                (size_t)fdt64_to_cpu(*(const dtb_aligned_uint64_t *)endp);
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
    memory_map->ram_start = (size_t)start_of_kernel;
    memory_map->kernel_start = (size_t)start_of_kernel;
    memory_map->kernel_end = (size_t)end_of_kernel;
    memory_map->ram_end =
        (memory_map->ram_start + (size_t)MEMORY_SIZE * 1024 * 1024);
    memory_map->dtb_file_start = 0;
    memory_map->dtb_file_end = 0;

    if (fdt_magic(dtb) != FDT_MAGIC)
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

    size_t base;
    size_t size;
    dtb_get_reg(dtb, offset, &base, &size);

    memory_map->ram_start = base;
    memory_map->ram_end = base + size;

    dtb_get_initrd(dtb, memory_map);
}

bool dtb_get_reg(void *dtb, int offset, size_t *base, size_t *size)
{
    uint32_t size_cells = dtb_get_size_cells(dtb);
    uint32_t address_cells = dtb_get_address_cells(dtb);

    int len;
    const uint32_t *value = fdt_getprop(dtb, offset, "reg", &len);
    if (value == NULL)
    {
        printk("dtb error\n");
        return false;
    }
    if (address_cells == 1)
    {
        // 32 bit address
        *base = fdt32_to_cpu(value[0]);
        value++;
    }
    else
    {
        // 64 bit address
        const dtb_aligned_uint64_t *address64 =
            (const dtb_aligned_uint64_t *)value;
        *base = fdt64_to_cpu(address64[0]);
        value += 2;
    }
    if (size_cells == 1)
    {
        // 32 bit size
        *size = fdt32_to_cpu(value[0]);
    }
    else
    {
        // 64 bit size
        const dtb_aligned_uint64_t *size64 =
            (const dtb_aligned_uint64_t *)value;
        *size = fdt64_to_cpu(size64[0]);
    }

    return true;
}

// note: gets called too early for printk...
uint64_t dtb_get_timebase(void *dtb)
{
    uint64_t fallback = 10000000ull;  // from qemu

    if (fdt_magic(dtb) != FDT_MAGIC)
    {
        return fallback;
    }

    int offset = fdt_path_offset(dtb, "/cpus");
    if (offset < 0)
    {
        // printk("dtb error: %s\n", (char *)fdt_strerror(offset));
        return fallback;
    }
    const uint32_t *value =
        fdt_getprop(dtb, offset, "timebase-frequency", NULL);
    if (value == NULL)
    {
        return fallback;
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

uint32_t dtb_get_size_cells(void *dtb)
{
    int root_offset = fdt_path_offset(dtb, "/");
    if (root_offset < 0) return 1;  // default

    const uint32_t *addr_prop =
        fdt_getprop(dtb, root_offset, "#size-cells", NULL);
    if (addr_prop == NULL) return 1;  // default

    return fdt32_to_cpu(*addr_prop);
}

uint32_t dtb_get_address_cells(void *dtb)
{
    int root_offset = fdt_path_offset(dtb, "/");
    if (root_offset < 0) return 1;  // default

    const uint32_t *addr_prop =
        fdt_getprop(dtb, root_offset, "#address-cells", NULL);
    if (addr_prop == NULL) return 1;  // default

    return fdt32_to_cpu(*addr_prop);
}

/// @brief Checks if an extension is part of the riscv_isa string
/// @param riscv_isa E.g. "rv64imafdc_zicsr_sstc"
/// @param ext
/// @return
bool extension_is_supported(const char *riscv_isa, const char *ext)
{
    riscv_isa += 4;  // first 4 chars are "rv32" or "rv64"
    // one char extensions are combined in the begining of the string:
    size_t ext_len = strlen(ext);
    if (ext_len == 1)
    {
        while (*riscv_isa != '_' && *riscv_isa != 0)
        {
            if (*riscv_isa == ext[0])
            {
                return true;
            }
            riscv_isa++;
        }
    }
    else
    {
        char *pos;
        while ((pos = strstr(riscv_isa, ext)) != NULL)
        {
            // potential match
            pos -= 1;  // move pointer back (there is always a valid char as
                       // riscv_isa was moved forward before)

            // found location does not have a '_' before it: found a match
            // inside of another ext ("ext" in "rv64imac_newext_foo")
            if (*pos != '_') continue;

            // also check end of _ext_ substring: can be '_' or end of string!
            if (pos[ext_len + 1] != '_' && pos[ext_len + 2] != 0) continue;

            return true;
        }
    }
    return false;
}

int dtb_get_cpu_offset(void *dtb, size_t cpu_id, bool print_errors)
{
    const size_t PATH_LEN = 16;
    char path_name[PATH_LEN];
    snprintf(path_name, PATH_LEN, "/cpus/cpu@%d", cpu_id);

    int offset = fdt_path_offset(dtb, path_name);
    if (offset < 0 && print_errors)
    {
        printk("dtb error: %s\n", (char *)fdt_strerror(offset));
    }
    return offset;
}

CPU_Features dtb_get_cpu_features(void *dtb, size_t cpu_id)
{
    CPU_Features featues = 0;

    int offset = dtb_get_cpu_offset(dtb, cpu_id, true);
    if (offset < 0) return 0;

    // parse MMU support
    int mmu_type_len;
    const char *mmu_type = fdt_getprop(dtb, offset, "mmu-type", &mmu_type_len);
    if (mmu_type != NULL)
    {
        if (strcmp(mmu_type, "riscv,sv32") == 0)
        {
            featues |= RV_SV32_SUPPORTED;
        }
        else if (strcmp(mmu_type, "riscv,sv39") == 0)
        {
            featues |= RV_SV39_SUPPORTED;
        }
        else if (strcmp(mmu_type, "riscv,sv48") == 0)
        {
            featues |= RV_SV48_SUPPORTED;
        }
        else if (strcmp(mmu_type, "riscv,sv57") == 0)
        {
            featues |= RV_SV57_SUPPORTED;
        }
    }

    // potentially relevant extensions
    int riscv_isa_len;
    const char *riscv_isa =
        fdt_getprop(dtb, offset, "riscv,isa", &riscv_isa_len);
    if (riscv_isa != NULL)
    {
        if (extension_is_supported(riscv_isa, "sstc"))
        {
            featues |= RV_EXT_SSTC;
        }
        if (extension_is_supported(riscv_isa, "f"))
        {
            featues |= RV_EXT_FLOAT;
        }
        if (extension_is_supported(riscv_isa, "d"))
        {
            featues |= RV_EXT_DOUBLE;
        }
    }

    return featues;
}
