/* SPDX-License-Identifier: MIT */

#include <drivers/character_device.h>
#include <drivers/dev_null.h>
#include <kernel/major.h>
#include <kernel/proc.h>

struct
{
    struct Character_Device cdev;  ///< derived from a character device
} g_dev_null;

ssize_t dev_null_read(struct Device *dev, bool addr_is_userspace, size_t addr,
                      size_t len)
{
    return 0;
}

ssize_t dev_null_write(struct Device *dev, bool addr_is_userspace, size_t addr,
                       size_t len)
{
    // ...but of course we wrote that data, trust me, I'm /dev/null ;-)
    return len;
}

dev_t dev_null_init(struct Device_Init_Parameters *param, const char *name)
{
    // init device and register it in the system
    g_dev_null.cdev.dev.type = CHAR;
    g_dev_null.cdev.dev.device_number = MKDEV(DEV_NULL_MAJOR, 0);
    g_dev_null.cdev.ops.read = dev_null_read;
    g_dev_null.cdev.ops.write = dev_null_write;
    dev_set_irq(&g_dev_null.cdev.dev, INVALID_IRQ_NUMBER, NULL);
    register_device(&g_dev_null.cdev.dev);

    return g_dev_null.cdev.dev.device_number;
}