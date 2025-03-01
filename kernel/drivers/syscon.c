/* SPDX-License-Identifier: MIT */

#include <drivers/mmio_access.h>
#include <drivers/syscon.h>
#include <kernel/major.h>
#include <kernel/reset.h>
#include <libfdt.h>

struct syscon
{
    bool is_initialized;
    size_t mmio_base;         ///< memory map start
    size_t poweroff_offset;   ///< expected: 0 (but read from device tree)
    size_t reboot_offset;     ///< expected: 0
    uint32_t poweroff_value;  ///< expected: 0x5555
    uint32_t reboot_value;    ///< expected: 0x7777
} g_syscon = {0};

void syscon_machine_power_off();
void syscon_machine_restart();

bool parse_dtb_node(void *dtb, const char *node_name,
                    const char *expected_comp_str, uint32_t *value_out,
                    size_t *offset_out)
{
    int offset = fdt_path_offset(dtb, node_name);
    if (offset < 0)
    {
        return false;
    }
    const char *comp_str = fdt_getprop(dtb, offset, "compatible", NULL);
    if (comp_str == NULL) return false;

    if (strcmp(comp_str, expected_comp_str) != 0)
    {
        return false;
    }

    // it's compatible, from now on complain if the dtb has unexpected data

    int error;
    const uint32_t *value_dtb = fdt_getprop(dtb, offset, "value", &error);
    if (value_dtb == NULL)
    {
        printk("dtb error parsing %s: %s\n", node_name,
               (char *)fdt_strerror(error));
        return false;
    }
    uint32_t value = fdt32_to_cpu(value_dtb[0]);
    *value_out = value;

    const uint32_t *offset_dtb = fdt_getprop(dtb, offset, "offset", &error);
    if (offset_dtb == NULL)
    {
        printk("dtb error parsing %s: %s\n", node_name,
               (char *)fdt_strerror(error));
        return false;
    }
    uint32_t register_offset = fdt32_to_cpu(offset_dtb[0]);
    *offset_out = (size_t)register_offset;

    return true;
}

bool parse_dtb_poweroff_node(void *dtb)
{
    return parse_dtb_node(dtb, "/poweroff", "syscon-poweroff",
                          &g_syscon.poweroff_value, &g_syscon.poweroff_offset);
}

bool parse_dtb_reboot_node(void *dtb)
{
    return parse_dtb_node(dtb, "/reboot", "syscon-reboot",
                          &g_syscon.reboot_value, &g_syscon.reboot_offset);
}

dev_t syscon_init(struct Device_Init_Parameters *init_parameters,
                  const char *name)
{
    if (g_syscon.is_initialized)
    {
        return INVALID_DEVICE;
    }

    if (!init_parameters || !init_parameters->dtb)
    {
        return INVALID_DEVICE;
    }

    if (!parse_dtb_poweroff_node(init_parameters->dtb))
    {
        return INVALID_DEVICE;
    }

    g_syscon.mmio_base = init_parameters->mem[0].start;
    printk("register syscon reboot/shutdown functions\n");
    g_machine_power_off_func = &syscon_machine_power_off;

    if (parse_dtb_reboot_node(init_parameters->dtb))
    {
        g_machine_restart_func = &syscon_machine_restart;
    }

    g_syscon.is_initialized = true;
    return MKDEV(SYSCON_MAJOR, 0);
}

void syscon_write_reg(size_t reg, uint32_t value)
{
    if (!g_syscon.is_initialized) return;

    MMIO_WRITE_UINT_32(g_syscon.mmio_base, reg, value);
}

void syscon_machine_power_off()
{
    syscon_write_reg(g_syscon.poweroff_offset, g_syscon.poweroff_value);
}

void syscon_machine_restart()
{
    syscon_write_reg(g_syscon.reboot_offset, g_syscon.reboot_value);
}
