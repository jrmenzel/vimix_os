/* SPDX-License-Identifier: MIT */

#include <arch/reset.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

#if defined(VIRT_TEST_BASE)

void machine_restart()
{
    (*(volatile uint32_t *)VIRT_TEST_BASE) = VIRT_TEST_REBOOT;
    panic("machine_restart() failed");
}

void machine_power_off()
{
    (*(volatile uint32_t *)VIRT_TEST_BASE) = VIRT_TEST_SHUTDOWN;
    panic("machine_power_off() failed");
}

#else

// fallback if virt-test is unavailable on this platform
void machine_restart() { panic("machine_restart() unsupported"); }
void machine_power_off() { panic("machine_power_off() unsupported"); }

#endif
