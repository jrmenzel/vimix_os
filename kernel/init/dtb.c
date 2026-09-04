/* SPDX-License-Identifier: MIT */

#include <drivers/device.h>
#include <drivers/driver_list.h>
#include <init/dtb.h>
#include <init/start.h>
#include <kernel/pgtable.h>
#include <lib/minmax.h>
#include <libfdt.h>

static bool dtb_read_prop_u32(const void *dtb, int node_offset,
                              const char *name, uint32_t *value_out)
{
    int len = 0;
    const uint32_t *value = fdt_getprop(dtb, node_offset, name, &len);
    if (value == NULL || len < (int)sizeof(uint32_t))
    {
        return false;
    }

    *value_out = fdt32_to_cpu(value[0]);
    return true;
}

int32_t dtb_read_prop_u32_with_fallback(const void *dtb, int node_offset,
                                        const char *name, int32_t fallback)
{
    uint32_t value = 0;
    if (dtb_read_prop_u32(dtb, node_offset, name, &value))
    {
        return (int32_t)value;
    }

    return fallback;
}

static bool dtb_read_prop_u64_or_u32(const void *dtb, int node_offset,
                                     const char *name, uint64_t *value_out)
{
    int len = 0;
    const void *prop = fdt_getprop(dtb, node_offset, name, &len);
    if (prop == NULL)
    {
        return false;
    }

    if (len >= (int)sizeof(uint64_t))
    {
        *value_out = fdt64_to_cpu(*(const dtb_aligned_uint64_t *)prop);
        return true;
    }

    if (len >= (int)sizeof(uint32_t))
    {
        *value_out = (uint64_t)fdt32_to_cpu(*(const uint32_t *)prop);
        return true;
    }

    return false;
}

bool dtb_read_u64_prop(const void *dtb, int node_offset, const char *name,
                       uint64_t *value_out)
{
    return dtb_read_prop_u64_or_u32(dtb, node_offset, name, value_out);
}

uint64_t dtb_read_prop_u64_with_fallback(const void *dtb, int node_offset,
                                         const char *name, uint64_t fallback)
{
    uint64_t value = 0;
    if (dtb_read_u64_prop(dtb, node_offset, name, &value))
    {
        return (uint64_t)value;
    }

    return fallback;
}

const char *dtb_get_nonempty_string_property(const void *dtb, int node_offset,
                                             const char *name, int *lenp_out)
{
    int len = 0;
    const char *value = fdt_getprop(dtb, node_offset, name, &len);
    if (value == NULL || len <= 0)
    {
        return NULL;
    }

    if (lenp_out != NULL)
    {
        *lenp_out = len;
    }
    return value;
}

static bool dtb_get_addr_size_cells(const void *dtb, int node_offset,
                                    int *addr_cells_out, int *size_cells_out)
{
    int parent_offset = fdt_parent_offset(dtb, node_offset);
    int addr_cells = fdt_address_cells(dtb, parent_offset);
    int size_cells = fdt_size_cells(dtb, parent_offset);

    if (addr_cells < 0 || size_cells < 0)
    {
        return false;
    }

    if (addr_cells > 2 || size_cells > 2)
    {
        return false;
    }

    *addr_cells_out = addr_cells;
    *size_cells_out = size_cells;
    return true;
}

static bool dtb_validate_cell_tuples_size(int byte_len, int tuple_cells)
{
    if (tuple_cells <= 0)
    {
        return false;
    }

    if (byte_len <= 0 || (byte_len % (int)sizeof(uint32_t)) != 0)
    {
        return false;
    }

    int total_cells = byte_len / (int)sizeof(uint32_t);
    return (total_cells % tuple_cells) == 0;
}

uint32_t *dtb_parse_cell(int cells_per_value, uint32_t *cells,
                         size_t *value_out);

static void dtb_parse_reg_tuple(uint32_t **cells, int addr_cells,
                                int size_cells, size_t *addr_out,
                                size_t *size_out)
{
    *cells = dtb_parse_cell(addr_cells, *cells, addr_out);
    *cells = dtb_parse_cell(size_cells, *cells, size_out);
}

bool dtb_is_str_in_str_list(const char *str_list, const char *str)
{
    size_t size_of_dev_str = strlen(str) + 1;

    // str_list is a list of nul-terminated strings
    while (true)
    {
        if (strncmp(str_list, str, size_of_dev_str) == 0)
        {
            return true;
        }
        str_list += strlen(str_list) + 1;
        if (str_list[0] == 0) break;
    }
    return false;
}

ssize_t dtb_add_driver_if_compatible(const void *dtb, const char *device_name,
                                     int device_offset,
                                     struct Devices_List *dev_list)
{
    for_each_driver(driver)
    {
        if ((driver->type == PHYSICAL) &&
            (dtb_is_str_in_str_list(device_name, driver->dtb_name)))
        {
            return dev_list_add_from_dtb(dev_list, dtb, device_name,
                                         device_offset, driver);
        }
    }

    return -1;
}

/// @brief Look for supported devices in the DTB and add them to the dev_list
/// @param dtb Device Tree
/// @param driver_list Known, supported drivers to look for in the DTB
/// @param dev_list Output list of found devices with init parameters read from
/// the DTB
void dtb_add_devices_to_dev_list(const void *dtb, struct Devices_List *dev_list)
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

        dtb_add_driver_if_compatible(dtb, value, off, dev_list);
    }
}

void dtb_get_initrd(const void *dtb, size_t *base, size_t *size)
{
    uint64_t initrd_begin = 0;
    uint64_t initrd_end = 0;
    int offset = fdt_path_offset(dtb, "/chosen");
    if (offset >= 0)
    {
        dtb_read_prop_u64_or_u32(dtb, offset, "linux,initrd-start",
                                 &initrd_begin);
        dtb_read_prop_u64_or_u32(dtb, offset, "linux,initrd-end", &initrd_end);
    }

    *base = (size_t)initrd_begin;
    *size = (size_t)(initrd_end - initrd_begin);
}

void dtb_get_memory(const void *dtb, size_t *base, size_t *size)
{
    int offset = fdt_path_offset(dtb, "/memory");
    if (offset < 0)
    {
        printk("dtb error: %s\n", (char *)fdt_strerror(offset));
        return;
    }

    dtb_get_reg(dtb, offset, base, size);

    if (*size == 0)
    {
        panic("No valid memory size read from device tree");
    }
}

uint32_t *dtb_parse_cell(int cells_per_value, uint32_t *cells,
                         size_t *value_out)
{
    if (cells_per_value != 0 && cells_per_value != 1 && cells_per_value != 2)
    {
        panic("dtb_parse_cell: invalid cells_per_value");
    }

    if (cells_per_value == 0)
    {
        *value_out = 0;
        return cells;
    }

    if (cells_per_value == 1)
    {
        *value_out = (size_t)fdt32_to_cpu(*cells);
        cells += 1;
    }

    if (cells_per_value == 2)
    {
        const dtb_aligned_uint64_t *cells_64 = (dtb_aligned_uint64_t *)(cells);
        uint64_t value = fdt64_to_cpu(*cells_64);
        *value_out = (size_t)value;
        cells += 2;
    }

    return cells;
}

struct Address_Range
{
    size_t child_addr;
    size_t parent_addr;
    size_t child_size;
};

#define MAX_ADDRESS_RANGES (8)

size_t get_address_ranges(const void *dtb, int parent_offset, int addr_cells,
                          int size_cells, struct Address_Range *range,
                          size_t range_array_size)

{
    memset(range, 0, sizeof(struct Address_Range) * range_array_size);
    size_t range_count = 0;

    int ranges_len;
    const uint32_t *ranges =
        fdt_getprop(dtb, parent_offset, "ranges", &ranges_len);
    if (ranges != NULL)
    {
        int p_parent_offset = fdt_parent_offset(dtb, parent_offset);

        int child_addr_cells = addr_cells;
        int parent_addr_cells = fdt_address_cells(dtb, p_parent_offset);
        int child_size_cells = size_cells;

        if (child_addr_cells < 0 || parent_addr_cells < 0 ||
            child_size_cells < 0)
        {
            return 0;
        }

        if (child_addr_cells > 2 || parent_addr_cells > 2 ||
            child_size_cells > 2)
        {
            return 0;
        }

        int range_tuple_cells =
            child_addr_cells + parent_addr_cells + child_size_cells;
        if (!dtb_validate_cell_tuples_size(ranges_len, range_tuple_cells))
        {
            return 0;
        }

        uint32_t *range_index = (uint32_t *)ranges;
        uint32_t *range_end = range_index + (ranges_len / sizeof(uint32_t));
        while ((range_index != range_end) && (range_count < range_array_size))
        {
            range_index = dtb_parse_cell(child_addr_cells, range_index,
                                         &(range[range_count].child_addr));
            range_index = dtb_parse_cell(parent_addr_cells, range_index,
                                         &(range[range_count].parent_addr));
            range_index = dtb_parse_cell(child_size_cells, range_index,
                                         &(range[range_count].child_size));
            range_count++;
        }
    }

    return range_count;
}

size_t map_mmio_address(size_t addr, struct Address_Range *range, size_t ranges)
{
    for (size_t i = 0; i < ranges; ++i)
    {
        if (addr >= range[i].child_addr &&
            (addr - range[i].child_addr) < range[i].child_size)
        {
            // addr falls within this address range
            // the mapping maps child_addr to parent_addr
            size_t offset = range[i].parent_addr - range[i].child_addr;
            return addr + offset;
        }
    }
    panic("map_mmio_address: can't map, address out of range");
    return addr;
}

bool dtb_get_regs(const void *dtb, int offset,
                  struct Device_Init_Parameters *params)
{
    int len;
    const char *regs_raw = fdt_getprop(dtb, offset, "reg", &len);
    if (regs_raw == NULL)
    {
        return false;
    }

    int len_names;
    const char *reg_names = fdt_getprop(dtb, offset, "reg-names", &len_names);

    int addr_cells = 0;
    int size_cells = 0;
    if (!dtb_get_addr_size_cells(dtb, offset, &addr_cells, &size_cells))
    {
        return false;
    }

    int reg_tuple_cells = addr_cells + size_cells;
    if (!dtb_validate_cell_tuples_size(len, reg_tuple_cells))
    {
        return false;
    }

    int parent_offset = fdt_parent_offset(dtb, offset);

    struct Address_Range range[MAX_ADDRESS_RANGES];
    size_t range_count = get_address_ranges(
        dtb, parent_offset, addr_cells, size_cells, range, MAX_ADDRESS_RANGES);

    uint32_t *reg_index = (uint32_t *)regs_raw;
    uint32_t *reg_end = reg_index + (len / sizeof(uint32_t));

    size_t map_idx = 0;
    while ((reg_index != reg_end) && (map_idx < DEVICE_MAX_MEM_MAPS))
    {
        // get address and size:
        dtb_parse_reg_tuple(&reg_index, addr_cells, size_cells,
                            &(params->mem[map_idx].start_pa),
                            &(params->mem[map_idx].size));

        if (range_count > 0)
        {
            // address mapping if the device tree stores bus local addresses
            // convert those to CPU mapped addresses
            params->mem[map_idx].start_pa = map_mmio_address(
                params->mem[map_idx].start_pa, range, range_count);
        }

        // get optional name:
        if (reg_names && len_names > 0)
        {
            params->mem[map_idx].name = reg_names;

            // reg_names is a list of len_names 0-terminated strings,
            // find the next one:
            while (len_names > 0 && *reg_names != 0)
            {
                reg_names++;
                len_names--;
            }

            if (len_names > 0)
            {
                reg_names++;
                len_names--;
            }
            else
            {
                reg_names = NULL;
            }
        }

        map_idx++;
    }

    params->mmu_map_memory = true;

    // might also have reg-shift
    params->reg_io_width = dtb_read_prop_u32_with_fallback(
        dtb, offset, "reg-io-width", params->reg_io_width);

    params->reg_shift = dtb_read_prop_u32_with_fallback(
        dtb, offset, "reg-shift", params->reg_shift);

    return true;
}

bool dtb_get_reg(const void *dtb, int offset, size_t *base, size_t *size)
{
    int address_cells = 0;
    int size_cells = 0;
    if (!dtb_get_addr_size_cells(dtb, offset, &address_cells, &size_cells))
    {
        printk("dtb error: invalid or unsupported cells\n");
        return false;
    }

    int len;
    const uint32_t *regs = fdt_getprop(dtb, offset, "reg", &len);
    if (regs == NULL)
    {
        printk("dtb error\n");
        return false;
    }

    if (!dtb_validate_cell_tuples_size(len, address_cells + size_cells))
    {
        printk("dtb error: malformed reg\n");
        return false;
    }

    uint32_t *reg_index = (uint32_t *)regs;
    dtb_parse_reg_tuple(&reg_index, address_cells, size_cells, base, size);

    return true;
}

// note: gets called too early for printk...
uint64_t dtb_get_timebase(const void *dtb)
{
    uint64_t fallback = 10000000ull;  // from qemu

    if (fdt_magic(dtb) != FDT_MAGIC)
    {
        return fallback;
    }

    int offset = fdt_path_offset(dtb, "/cpus");
    if (offset < 0)
    {
        return fallback;
    }

    return (uint64_t)dtb_read_prop_u32_with_fallback(
        dtb, offset, "timebase-frequency", fallback);
}

int dtb_find_boot_console_index(const void *dtb)
{
    // find /chosen/stdout-path
    int offset = fdt_path_offset(dtb, "/chosen");
    if (offset < 0) return offset;  // contains a negative error code

    int lenp = 0;  // string length incl. 0-terminator
    const char *console =
        dtb_get_nonempty_string_property(dtb, offset, "stdout-path", &lenp);
    if (console == NULL) return -1;

#define MAX_NAME_LEN 64
    char name[MAX_NAME_LEN];
    if (lenp <= 0)
    {
        return -1;
    }
    size_t copy_len = (size_t)lenp;
    if (copy_len >= MAX_NAME_LEN)
    {
        copy_len = MAX_NAME_LEN - 1;
    }
    memcpy(name, console, copy_len);
    name[copy_len] = 0;

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

    return fdt_path_offset(dtb, name);
}

struct Found_Device *dtb_find_boot_console_in_dev_list(
    const void *dtb, struct Devices_List *dev_list)
{
    int console_offset = dtb_find_boot_console_index(dtb);
    if (console_offset >= 0)
    {
        // See what the selected console is compatible with and find its
        // driver-backed device.
        const char *value = dtb_get_nonempty_string_property(
            dtb, console_offset, "compatible", NULL);
        if (value != NULL)
        {
            struct list_head *pos;
            list_for_each(pos, &dev_list->devices)
            {
                struct Found_Device *dev = found_device_from_devices_list(pos);
                if (dtb_is_str_in_str_list(value, dev->driver->dtb_name))
                {
                    return dev;
                }
            }
        }
    }

    // Spike advertises an HTIF device but commonly has no stdout-path (or a
    // path naming a UART whose driver is intentionally disabled). Use HTIF as
    // the boot console when it is available.
    struct list_head *pos;
    list_for_each(pos, &dev_list->devices)
    {
        struct Found_Device *dev = found_device_from_devices_list(pos);
        if (strcmp(dev->driver->dtb_name, "ucb,htif0") == 0)
        {
            return dev;
        }
    }

    return NULL;
}

int dtb_get_cpu_offset(const void *dtb, size_t cpu_id, bool print_errors)
{
    char path_name[32];
    snprintf(path_name, sizeof(path_name), "/cpus/cpu@%zu", cpu_id);

    int offset = fdt_path_offset(dtb, path_name);
    if (offset < 0 && print_errors)
    {
        printk("dtb error: %s\n", (char *)fdt_strerror(offset));
    }
    return offset;
}

const char *dtb_cpus_enable_method(const void *dtb)
{
    int cpus_offset = fdt_path_offset(dtb, "/cpus");
    if (cpus_offset < 0)
    {
        return NULL;
    }

    return dtb_get_nonempty_string_property(dtb, cpus_offset, "enable-method",
                                            NULL);
}

bool parse_dtb_node(const void *dtb, const char *node_name,
                    const char *expected_comp_str, uint32_t *value_out,
                    size_t *offset_out)
{
    int offset = fdt_path_offset(dtb, node_name);
    if (offset < 0)
    {
        return false;
    }
    const char *comp_str =
        dtb_get_nonempty_string_property(dtb, offset, "compatible", NULL);
    if (comp_str == NULL) return false;

    if (strcmp(comp_str, expected_comp_str) != 0)
    {
        return false;
    }

    // it's compatible, from now on complain if the dtb has unexpected data

    uint32_t value = 0;
    if (!dtb_read_prop_u32(dtb, offset, "value", &value))
    {
        printk("dtb error parsing %s\n", node_name);
        return false;
    }
    *value_out = value;

    uint32_t register_offset = 0;
    if (!dtb_read_prop_u32(dtb, offset, "offset", &register_offset))
    {
        printk("dtb error parsing %s\n", node_name);
        return false;
    }
    *offset_out = (size_t)register_offset;

    return true;
}
