/* SPDX-License-Identifier: MIT */

#include <drivers/character_device.h>
#include <drivers/dev_zero.h>
#include <kernel/major.h>
#include <kernel/proc.h>

struct
{
    struct Character_Device cdev;  ///< derived from a character device
} g_dev_zero;

ssize_t dev_zero_read(struct Device *dev, bool addr_is_userspace, size_t addr,
                      size_t len, uint32_t unused_file_offset)
{
    char zero = 0;
    for (size_t i = 0; i < len; ++i)
    {
        int32_t ret = either_copyout(addr_is_userspace, addr + i, &zero, 1);
        if (ret == -1) return -1;
    }

    return len;
}

ssize_t dev_zero_write(struct Device *dev, bool addr_is_userspace, size_t addr,
                       size_t len)
{
    return len;
}

dev_t dev_zero_init(struct Device_Init_Parameters *param, const char *name)
{
    // init device and register it in the system
    g_dev_zero.cdev.dev.name = "zero";
    g_dev_zero.cdev.dev.type = CHAR;
    g_dev_zero.cdev.dev.device_number = MKDEV(DEV_ZERO_MAJOR, 0);
    g_dev_zero.cdev.ops.read = dev_zero_read;
    g_dev_zero.cdev.ops.write = dev_zero_write;
    g_dev_zero.cdev.ops.ioctl = NULL;
    dev_set_irq(&g_dev_zero.cdev.dev, INVALID_IRQ_NUMBER, NULL);
    register_device(&g_dev_zero.cdev.dev);

    return g_dev_zero.cdev.dev.device_number;
}