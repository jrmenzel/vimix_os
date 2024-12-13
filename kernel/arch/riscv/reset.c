/* SPDX-License-Identifier: MIT */

#include <arch/reset.h>
#include <drivers/syscon.h>
#include <kernel/kernel.h>

#ifdef __ENABLE_SBI__
#include <arch/riscv/sbi.h>
#endif

void machine_restart()
{
#ifdef __ENABLE_SBI__
    sbi_system_reset(SBI_SRST_TYPE_WARM_REBOOT, SBI_SRST_REASON_NONE);
#endif
    syscon_write_reg(VIRT_TEST_SHUTDOWN_REG, VIRT_TEST_REBOOT);

    panic("machine_restart() failed");
}

void machine_power_off()
{
#ifdef __ENABLE_SBI__
    sbi_system_reset(SBI_SRST_TYPE_SHUTDOWN, SBI_SRST_REASON_NONE);
#endif
    syscon_write_reg(VIRT_TEST_SHUTDOWN_REG, VIRT_TEST_SHUTDOWN);

    panic("machine_power_off() failed");
}
