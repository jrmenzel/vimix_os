/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>
#include <kernel/reset.h>

void (*g_machine_restart_func)() = NULL;
void (*g_machine_power_off_func)() = NULL;

void machine_restart()
{
    if (g_machine_restart_func)
    {
        g_machine_restart_func();
    }

    printk("machine_restart() failed, try shutdown...\n");
    machine_power_off();
}

void machine_power_off()
{
    if (g_machine_power_off_func)
    {
        g_machine_power_off_func();
    }

    panic("machine_power_off() failed");
}
