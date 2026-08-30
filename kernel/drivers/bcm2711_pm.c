/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <drivers/bcm2711_pm.h>
#include <drivers/mmio_access.h>
#include <kernel/major.h>
#include <kernel/reset.h>
#include <kernel/spinlock.h>

REGISTER_DRIVER("brcm,bcm2711-pm", bcm2711_pm_init);
REGISTER_DRIVER("brcm,bcm2835-pm-wdt", bcm2711_pm_init);

struct bcm2711_pm
{
    struct spinlock lock;
    size_t mmio_base;  ///< memory map start
    bool isInitialized;
};

struct bcm2711_pm g_bcm2711_pm = {0};

#define PM_RSTC_OFFSET 0x1c
#define PM_WDOG_OFFSET 0x24

#define PM_PASSWORD 0x5A000000u
#define PM_RSTC_WRCFG_MASK 0x00000030u
#define PM_RSTC_WRCFG_FULL_RESET 0x00000020u

// a tiny timeout is enough, if the reset path works this will trigger quickly
#define PM_WDOG_TICKS 10u

void bcm2711_pm_machine_restart();

static void bcm2711_pm_do_full_reset(void)
{
    spin_lock(&g_bcm2711_pm.lock);

    MMIO_WRITE_UINT_32(g_bcm2711_pm.mmio_base, PM_WDOG_OFFSET,
                       PM_PASSWORD | PM_WDOG_TICKS);

    uint32_t rstc = MMIO_READ_UINT_32(g_bcm2711_pm.mmio_base, PM_RSTC_OFFSET);
    rstc &= ~PM_RSTC_WRCFG_MASK;
    rstc |= PM_RSTC_WRCFG_FULL_RESET;
    MMIO_WRITE_UINT_32(g_bcm2711_pm.mmio_base, PM_RSTC_OFFSET,
                       PM_PASSWORD | rstc);

    spin_unlock(&g_bcm2711_pm.lock);
}

dev_t bcm2711_pm_init(struct Device_Init_Parameters *init_param,
                      const char *name)
{
    spin_lock_init(&g_bcm2711_pm.lock, "bcm2711_pm_lock");
    g_bcm2711_pm.mmio_base = init_param->mem[0].start_va;
    g_bcm2711_pm.isInitialized = true;

    printk("register BCM2711 PM shutdown/restart functions\n");
    g_machine_restart_func = &bcm2711_pm_machine_restart;

    return MKDEV(BCM2711_PM_MAJOR, 0);
}

void bcm2711_pm_machine_restart()
{
    if (!g_bcm2711_pm.isInitialized)
    {
        // can't restart the machine
        return;
    }

    bcm2711_pm_do_full_reset();

    // if reset did not trigger, avoid spinning hot
    while (true)
    {
        wait_for_interrupt();
    }
}
