/* SPDX-License-Identifier: MIT */

#include <arch/asm.h>
#include <drivers/bcm/bcm2835_gpio.h>
#include <drivers/driver.h>
#include <kernel/string.h>

REGISTER_DRIVER("brcm,bcm2835-gpio", bcm2835_gpio_init);
REGISTER_DRIVER("brcm,bcm2711-gpio", bcm2835_gpio_init);
REGISTER_DRIVER("brcm,bcm2835-gpiomem", bcm2835_gpio_init);

struct bcm2835_gpio g_bcm2835_gpio = {0};

// docs: https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf

// each function select register controls 9 GPIO pins with 3 bits each (see
// below)
#define GPFSEL_PINS_PER_REG (9)
#define GPFSEL0 (0x00)
#define GPFSEL1 (0x04)
#define GPFSEL2 (0x08)
#define GPFSEL3 (0x0C)
#define GPFSEL4 (0x10)
#define GPFSEL5 (0x14)

#define GPSET0 (0x1C)
#define GPSET1 (0x20)
#define GPCLR0 (0x28)
#define GPLEV0 (0x34)
#define GPLEV1 (0x38)
#define GPEDS0 (0x40)
#define GPEDS1 (0x44)
#define GPHEN0 (0x64)
#define GPHEN1 (0x68)
#define GPPUD (0x94)
#define GPPUDCLK0 (0x98)
#define GPPUDCLK1 (0x9C)
// bcm2711 (Pi4): replacement for GPPUD/GPPUDCLKx with 2 bits per pin
#define GPPUPPDN0 (0xE4)

#define MAX_PINS (GPFSEL_PINS_PER_REG * 6)

dev_t bcm2835_gpio_init(struct Device_Init_Parameters *init_parameters,
                        const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    spin_lock_init(&g_bcm2835_gpio.lock, "bcm2835_gpio_lock");

    g_bcm2835_gpio.mmio_base = init_parameters->mem[0].start_va;
    g_bcm2835_gpio.is_2711_variant = (strcmp(name, "brcm,bcm2711-gpio") == 0);
    g_bcm2835_gpio.is_initialized = true;
    return MKDEV(BCM2835_GPIO_MAJOR, 0);
}

void bcm2835_gpio_set_pin_to_function(uint32_t pin, uint32_t func)
{
    if (!g_bcm2835_gpio.is_initialized) return;
    if (pin >= MAX_PINS) return;  // only 6 registers with 9 values each
    if (func > 8) return;         // ony 3 bit per function

    spin_lock(&g_bcm2835_gpio.lock);

    size_t reg_idx = (pin / GPFSEL_PINS_PER_REG) * sizeof(uint32_t);
    size_t pin_shift = (pin % GPFSEL_PINS_PER_REG) * 3;

    uint32_t value =
        MMIO_READ_UINT_32(g_bcm2835_gpio.mmio_base, GPFSEL0 + reg_idx);
    uint32_t pin_mask = 7 << pin_shift;
    // clear all 3 bits of this pin:
    value &= ~pin_mask;
    // set new function:
    value |= func << pin_shift;
    MMIO_WRITE_UINT_32(g_bcm2835_gpio.mmio_base, GPFSEL0 + reg_idx, value);

    spin_unlock(&g_bcm2835_gpio.lock);
}

void bcm2835_gpio_set_pull_up_control(uint32_t pin, uint32_t ctl)
{
    if (!g_bcm2835_gpio.is_initialized) return;
    if (pin >= MAX_PINS) return;  // only 6 registers with 9 values each
    if (ctl >= 3) return;         // ony 2 bit and 3 is reserved

    spin_lock(&g_bcm2835_gpio.lock);

    if (g_bcm2835_gpio.is_2711_variant)
    {
        size_t reg_idx = (pin / 16) * sizeof(uint32_t);
        size_t pin_shift = (pin % 16) * 2;

        uint32_t value =
            MMIO_READ_UINT_32(g_bcm2835_gpio.mmio_base, GPPUPPDN0 + reg_idx);
        uint32_t pin_mask = 3 << pin_shift;
        value &= ~pin_mask;
        value |= ctl << pin_shift;
        MMIO_WRITE_UINT_32(g_bcm2835_gpio.mmio_base, GPPUPPDN0 + reg_idx,
                           value);
    }
    else
    {
        // BCM2835 way with separate clock registers:
        // write to control register:
        size_t reg_idx = (pin / 32) * sizeof(uint32_t);
        size_t pin_shift = pin % 32;

        MMIO_WRITE_UINT_32(g_bcm2835_gpio.mmio_base, GPPUD, ctl);
        ARCH_ASM_WAIT_CLOCKS(150);
        MMIO_WRITE_UINT_32(g_bcm2835_gpio.mmio_base, GPPUDCLK0 + reg_idx,
                           1 << pin_shift);
        ARCH_ASM_WAIT_CLOCKS(150);
        // write to both to flush:
        MMIO_WRITE_UINT_32(g_bcm2835_gpio.mmio_base, GPPUD, 0);
        MMIO_WRITE_UINT_32(g_bcm2835_gpio.mmio_base, GPPUDCLK0 + reg_idx, 0);
    }

    spin_unlock(&g_bcm2835_gpio.lock);
}
