/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>
#include <kernel/spinlock.h>

struct bcm2835_aux
{
    struct spinlock lock;
    size_t mmio_base;  ///< memory map start
    bool is_initialized;
};

dev_t bcm2835_aux_init(struct Device_Init_Parameters *init_param,
                       const char *name);

#define BCM2835_AUX_DEVICE_UART (0)
#define BCM2835_AUX_DEVICE_SPI1 (1)
#define BCM2835_AUX_DEVICE_SPI2 (2)

bool bcm2835_aux_enable(int32_t device);
