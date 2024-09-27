/* SPDX-License-Identifier: MIT */

#include <arch/reset.h>
#include <drivers/syscon.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

void machine_restart()
{
    syscon_write_reg(VIRT_TEST_SHUTDOWN_REG, VIRT_TEST_REBOOT);
    panic("machine_restart() failed");
}

void machine_power_off()
{
    syscon_write_reg(VIRT_TEST_SHUTDOWN_REG, VIRT_TEST_SHUTDOWN);
    panic("machine_power_off() failed");
}
