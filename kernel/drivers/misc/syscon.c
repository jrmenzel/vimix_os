/* SPDX-License-Identifier: MIT */

#include <drivers/driver.h>
#include <drivers/misc/syscon.h>
#include <init/dtb.h>
#include <kernel/pgtable.h>
#include <kernel/reset.h>

REGISTER_DRIVER("syscon", syscon_init);

struct syscon_device
{
    size_t mmio_base;         ///< memory map start
    size_t poweroff_offset;   ///< expected: 0 (but read from device tree)
    size_t reboot_offset;     ///< expected: 0
    uint32_t poweroff_value;  ///< expected: 0x5555
    uint32_t reboot_value;    ///< expected: 0x7777
};

struct syscon_device *g_syscon = NULL;

void syscon_machine_power_off();
void syscon_machine_restart();

bool parse_dtb_poweroff_node(struct syscon_device *syscon, const void *dtb)
{
    return parse_dtb_node(dtb, "/poweroff", "syscon-poweroff",
                          &syscon->poweroff_value, &syscon->poweroff_offset);
}

bool parse_dtb_reboot_node(struct syscon_device *syscon, const void *dtb)
{
    return parse_dtb_node(dtb, "/reboot", "syscon-reboot",
                          &syscon->reboot_value, &syscon->reboot_offset);
}

dev_t syscon_init(struct Device_Init_Parameters *init_parameters,
                  const char *name)
{
    DRIVER_CHECK_INIT_PARAMS_DTB(init_parameters);

    if (g_syscon != NULL)
    {
        // only one instance is supported
        return INVALID_DEVICE;
    }

    g_syscon = kmalloc(sizeof(struct syscon_device), ALLOC_FLAG_ZERO_MEMORY);
    if (g_syscon == NULL)
    {
        return INVALID_DEVICE;
    }

    if (!parse_dtb_poweroff_node(g_syscon, init_parameters->dtb))
    {
        kfree(g_syscon);
        g_syscon = NULL;
        return INVALID_DEVICE;
    }

    g_syscon->mmio_base = init_parameters->mem[0].start_va;
    g_machine_power_off_func = &syscon_machine_power_off;

    if (parse_dtb_reboot_node(g_syscon, init_parameters->dtb))
    {
        g_machine_restart_func = &syscon_machine_restart;
    }

    return MKDEV(SYSCON_MAJOR, 0);
}

void syscon_machine_power_off()
{
    MMIO_WRITE_UINT_32(g_syscon->mmio_base, g_syscon->poweroff_offset,
                       g_syscon->poweroff_value);
}

void syscon_machine_restart()
{
    MMIO_WRITE_UINT_32(g_syscon->mmio_base, g_syscon->reboot_offset,
                       g_syscon->reboot_value);
}
