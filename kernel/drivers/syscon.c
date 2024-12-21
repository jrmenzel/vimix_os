/* SPDX-License-Identifier: MIT */

#include <drivers/syscon.h>
#include <kernel/major.h>
#include <kernel/reset.h>

struct Device_Init_Parameters syscon_params = {0};
bool syscon_is_initialized = false;

void syscon_machine_power_off();
void syscon_machine_restart();

dev_t syscon_init(struct Device_Init_Parameters *init_parameters,
                  const char *name)
{
    if (syscon_is_initialized)
    {
        return INVALID_DEVICE;
    }
    syscon_params = *init_parameters;
    printk("register syscon reboot/shutdown functions\n");
    g_machine_power_off_func = &syscon_machine_power_off;
    g_machine_restart_func = &syscon_machine_restart;
    syscon_is_initialized = true;
    return MKDEV(SYSCON_MAJOR, 0);
}

void syscon_write_reg(size_t reg, uint32_t value)
{
    if (!syscon_is_initialized) return;

    (*(volatile uint32_t *)syscon_params.mem_start) = value;
}

void syscon_machine_power_off()
{
    syscon_write_reg(VIRT_TEST_SHUTDOWN_REG, VIRT_TEST_SHUTDOWN);
}

void syscon_machine_restart()
{
    syscon_write_reg(VIRT_TEST_SHUTDOWN_REG, VIRT_TEST_REBOOT);
}
