/* SPDX-License-Identifier: MIT */

#include <drivers/character_device.h>
#include <drivers/jh7110_syscrg.h>
#include <drivers/jh7110_temp.h>
#include <drivers/mmio_access.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>

#define SFCTEMP_RSTN (1 << 0)  // 0: reset,    1: de-assert
#define SFCTEMP_PD (1 << 1)    // 0: power up, 1: power down
#define SFCTEMP_RUN (1 << 2)   // 0: disable,  1: enable
#define SFCTEMP_DOUT_MSK 0xFFF0000
#define SFCTEMP_DOUT_POS 16
// DOUT to Celcius conversion constants
#define SFCTEMP_Y1000 237500L
#define SFCTEMP_Z 4094L
#define SFCTEMP_K1000 81100L

struct
{
    struct Character_Device cdev;  ///< derived from a character device
    bool is_initialized;
    size_t mmio_base;  ///< memory map start
} g_jh7110_temp = {0};

ssize_t copy_out_int(size_t value, bool addr_is_userspace, size_t addr,
                     size_t len, size_t str_offset)
{
    const size_t MAX_BUF = 16;
    char string_buffer[MAX_BUF];
    size_t str_len = snprintf(string_buffer, MAX_BUF, "%zd\n", value);
    string_buffer[str_len++] = 0;

    if (str_len <= str_offset)
    {
        return 0;  // EOF
    }
    str_len = str_len - str_offset;

    size_t copy_len = (str_len > len) ? len : str_len;
    if (either_copyout(addr_is_userspace, addr, string_buffer + str_offset,
                       copy_len) < 0)
    {
        return -1;
    }

    return copy_len;
}

ssize_t jh7110_temp_read(struct Device *dev, bool addr_is_userspace,
                         size_t addr, size_t len, uint32_t file_offset)
{
    uint32_t dout = MMIO_READ_UINT_32(g_jh7110_temp.mmio_base, 0);
    dout &= SFCTEMP_DOUT_MSK;
    dout >>= SFCTEMP_DOUT_POS;  // Shift to get the output value

    // Calculate the temperature in millidegrees Celsius (mC)
    long temp = (long)dout * SFCTEMP_Y1000 / SFCTEMP_Z - SFCTEMP_K1000;

    return copy_out_int(temp, addr_is_userspace, addr, len, file_offset);
}

void jh7110_temp_interrupt(dev_t dev) {}

dev_t jh7110_temp_init(struct Device_Init_Parameters *init_parameters,
                       const char *name)
{
    if (g_jh7110_temp.is_initialized || !init_parameters)
    {
        printk("temp: already initialized\n");
        return INVALID_DEVICE;
    }

    g_jh7110_temp.mmio_base = init_parameters->mem[0].start;

    jh7110_syscrg_enable(SYSCLK_TEMP_APB);
    jh7110_syscrg_enable(SYSCLK_TEMP_CORE);

    jh7110_syscrg_deassert(RSTN_TEMP_APB);
    jh7110_syscrg_deassert(RSTN_TEMP_CORE);

    MMIO_WRITE_UINT_8(g_jh7110_temp.mmio_base, 0,
                      SFCTEMP_PD);  // Power down the thermal sensor
    MMIO_WRITE_UINT_8(g_jh7110_temp.mmio_base, 0, 0);  // Power Up
    MMIO_WRITE_UINT_8(g_jh7110_temp.mmio_base, 0,
                      SFCTEMP_RSTN);  // De-assert reset
    // enable thermal sensor
    MMIO_WRITE_UINT_8(g_jh7110_temp.mmio_base, 0, SFCTEMP_RUN | SFCTEMP_RSTN);

    // init device and register it in the system
    g_jh7110_temp.cdev.dev.name = "temp";
    g_jh7110_temp.cdev.dev.type = CHAR;
    g_jh7110_temp.cdev.dev.device_number = MKDEV(JH7110_TEMP_MAJOR, 0);
    g_jh7110_temp.cdev.ops.read = jh7110_temp_read;
    g_jh7110_temp.cdev.ops.write = character_device_write_unsupported;
    dev_set_irq(&g_jh7110_temp.cdev.dev, init_parameters->interrupt,
                jh7110_temp_interrupt);
    register_device(&g_jh7110_temp.cdev.dev);

    g_jh7110_temp.is_initialized = true;
    return g_jh7110_temp.cdev.dev.device_number;
}
