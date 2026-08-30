/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>
#include <kernel/spinlock.h>

struct bcm2835_gpio
{
    struct spinlock lock;
    size_t mmio_base;  ///< memory map start
    bool is_2711_variant;
    bool is_initialized;
};

dev_t bcm2835_gpio_init(struct Device_Init_Parameters *init_param,
                        const char *name);

#define GPFSEL_FUNC_INPUT (0)
#define GPFSEL_FUNC_OUTPUT (1)
#define GPFSEL_FUNC_ALT_0 (4)
#define GPFSEL_FUNC_ALT_1 (5)
#define GPFSEL_FUNC_ALT_2 (6)
#define GPFSEL_FUNC_ALT_3 (7)
#define GPFSEL_FUNC_ALT_4 (3)
#define GPFSEL_FUNC_ALT_5 (2)

void bcm2835_gpio_set_pin_to_function(uint32_t pin, uint32_t func);

#define GPPUD_OFF (0)
#define GPPUD_ENABLE_PULL_DOWN (1)
#define GPPUD_ENABLE_PULL_UP (2)
void bcm2835_gpio_set_pull_up_control(uint32_t pin, uint32_t ctl);
