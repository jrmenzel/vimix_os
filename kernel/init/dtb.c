/* SPDX-License-Identifier: MIT */

#include <arch/platform.h>
#include <drivers/device.h>
#include <init/dtb.h>
#include <init/main.h>
#include <lib/minmax.h>
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

    if (size == 0)
    {
        panic("No valid memory size read from device tree");
    }

    memory_map->ram_start = base;
    memory_map->ram_end = base + size;

    dtb_get_initrd(dtb, memory_map);
}

uint32_t *dtb_parse_cell(int cells_per_value, uint32_t *cells,
                         size_t *value_out)
{
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

#define MAX_ADDRESS_RAGES (8)

size_t get_address_ranges(void *dtb, int parent_offset, int addr_cells,
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
        if (addr > range[i].child_addr &&
            addr < (range[i].child_addr + range[i].child_size))
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

bool dtb_get_regs(void *dtb, int offset, struct Device_Init_Parameters *params)
{
    int len;
    const char *regs_raw = fdt_getprop(dtb, offset, "reg", &len);
    if (regs_raw == NULL) return false;

    int len_names;
    const char *reg_names = fdt_getprop(dtb, offset, "reg-names", &len_names);

    int parent_offset = fdt_parent_offset(dtb, offset);
    int addr_cells = fdt_address_cells(dtb, parent_offset);
    int size_cells = fdt_size_cells(dtb, parent_offset);

    struct Address_Range range[MAX_ADDRESS_RAGES];
    size_t range_count = get_address_ranges(
        dtb, parent_offset, addr_cells, size_cells, range, MAX_ADDRESS_RAGES);

    uint32_t *reg_index = (uint32_t *)regs_raw;
    uint32_t *reg_end = reg_index + (len / sizeof(uint32_t));

    size_t map_idx = 0;
    while ((reg_index != reg_end) && (map_idx < DEVICE_MAX_MEM_MAPS))
    {
        // get address and size:
        reg_index = dtb_parse_cell(addr_cells, reg_index,
                                   &(params->mem[map_idx].start));
        reg_index =
            dtb_parse_cell(size_cells, reg_index, &(params->mem[map_idx].size));

        if (range_count > 0)
        {
            // address mapping if the device tree stores bus local addresses
            // convert those to CPU mapped addresses
            params->mem[map_idx].start = map_mmio_address(
                params->mem[map_idx].start, range, range_count);
        }

        // get optional name:
        if (reg_names && len_names > 0)
        {
            params->mem[map_idx].name = reg_names;

            // reg_names is a list of len_names 0-terminated strings,
            // find the next one:
            do {
                reg_names++;
                len_names--;
            } while (*reg_names != 0);
            reg_names++;
            len_names--;
        }

        map_idx++;
    }

    params->mmu_map_memory = true;

    // might also have reg-shift
    params->reg_io_width = dtb_getprop32_with_fallback(
        dtb, offset, "reg-io-width", params->reg_io_width);

    params->reg_shift = dtb_getprop32_with_fallback(dtb, offset, "reg-shift",
                                                    params->reg_shift);

    return true;
}

bool dtb_get_reg(void *dtb, int offset, size_t *base, size_t *size)
{
    int parent_offset = fdt_parent_offset(dtb, offset);
    uint32_t address_cells = fdt_address_cells(dtb, parent_offset);
    uint32_t size_cells = fdt_size_cells(dtb, parent_offset);

    int len;
    const uint32_t *regs = fdt_getprop(dtb, offset, "reg", &len);
    if (regs == NULL)
    {
        printk("dtb error\n");
        return false;
    }
    uint32_t *reg_index = (uint32_t *)regs;
    reg_index = dtb_parse_cell(address_cells, reg_index, base);
    reg_index = dtb_parse_cell(size_cells, reg_index, size);

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

ssize_t dtb_find_boot_console_in_dev_list(void *dtb,
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

    // find the device:
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        if (strcmp(value, dev_list->dev[i].driver->dtb_name) == 0)
        {
            return i;
        }
    }

    return -1;
}

int32_t dtb_getprop32_with_fallback(const void *dtb, int node_offset,
                                    const char *name, int32_t fallback)
{
    const int32_t *intp = (int32_t *)fdt_getprop(dtb, node_offset, name, NULL);
    if (intp != NULL)
    {
        int32_t value = fdt32_to_cpu(*intp);
        return value;
    }

    return fallback;
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

            // also check end of _ext_ substring: can be '_' or end of
            // string!
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
#if defined(__RISCV_EXT_SSTC)
        if (extension_is_supported(riscv_isa, "sstc"))
        {
            featues |= RV_EXT_SSTC;
        }
#endif
        if (extension_is_supported(riscv_isa, "f"))
        {
            featues |= RV_EXT_FLOAT;
        }
        if (extension_is_supported(riscv_isa, "d"))
        {
            featues |= RV_EXT_DOUBLE;
        }
    }

    int riscv_isa_ext_len;
    const char *riscv_isa_ext =
        fdt_getprop(dtb, offset, "riscv,isa-extensions", &riscv_isa_ext_len);
    if (riscv_isa_ext != NULL)
    {
        while (true)
        {
            // printk("ext: %s\n", riscv_isa_ext);
#if defined(__RISCV_EXT_SSTC)
            if (strcmp(riscv_isa_ext, "sstc"))
            {
                featues |= RV_EXT_SSTC;
            }
#endif
            if (strcmp(riscv_isa_ext, "f"))
            {
                featues |= RV_EXT_FLOAT;
            }
            if (strcmp(riscv_isa_ext, "d"))
            {
                featues |= RV_EXT_DOUBLE;
            }

            riscv_isa_ext += strlen(riscv_isa_ext) + 1;
            if (riscv_isa_ext[0] == 0) break;
        }
    }

    return featues;
}
