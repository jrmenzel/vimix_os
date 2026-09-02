/* SPDX-License-Identifier: MIT */

#include <drivers/bcm/bcm2835_aux.h>
#include <drivers/driver.h>

REGISTER_DRIVER("brcm,bcm2835-aux", bcm2835_aux_init);

struct bcm2835_aux g_bcm2835_aux = {0};

// see https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf page 9

#define AUXIRQ 0x00
#define AUXENB 0x04

dev_t bcm2835_aux_init(struct Device_Init_Parameters *init_parameters,
                       const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    spin_lock_init(&g_bcm2835_aux.lock, "bcm2835_aux_lock");

    g_bcm2835_aux.mmio_base = init_parameters->mem[0].start_va;
    g_bcm2835_aux.is_initialized = true;
    return MKDEV(BCM2835_AUX_MAJOR, 0);
}

bool bcm2835_aux_enable(int32_t device)
{
    if (g_bcm2835_aux.is_initialized == false) return false;
    if (device > 2) return false;

    spin_lock(&g_bcm2835_aux.lock);

    uint32_t register_state =
        MMIO_READ_UINT_32(g_bcm2835_aux.mmio_base, AUXENB);
    register_state |= (1 << device);
    MMIO_WRITE_UINT_32(g_bcm2835_aux.mmio_base, AUXENB, register_state);

    spin_unlock(&g_bcm2835_aux.lock);
    return true;
}
