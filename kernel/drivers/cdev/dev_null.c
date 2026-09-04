/* SPDX-License-Identifier: MIT */

#include <arch/irq.h>
#include <drivers/cdev/character_device.h>
#include <drivers/cdev/dev_null.h>
#include <drivers/driver.h>
#include <kernel/proc.h>

REGISTER_VIRTUAL_DRIVER("/dev/null", dev_null_init);

struct
{
    struct Character_Device cdev;  ///< derived from a character device
} g_dev_null;

ssize_t dev_null_read(struct Device *dev, bool addr_is_userspace, size_t addr,
                      size_t len, uint32_t unused_file_offset)
{
    return 0;
}

ssize_t dev_null_write(struct Device *dev, bool addr_is_userspace, size_t addr,
                       size_t len)
{
    // ...but of course we wrote that data, trust me, I'm /dev/null ;-)
    return len;
}

dev_t dev_null_init(struct Device_Init_Parameters *init_parameters,
                    const char *name)
{
    // init device and register it in the system
    dev_init(&g_dev_null.cdev.dev, CHAR, MKDEV(DEV_NULL_MAJOR, 0), "null",
             init_parameters->interrupts, init_parameters->interrupt_count,
             NULL);
    g_dev_null.cdev.ops.read = dev_null_read;
    g_dev_null.cdev.ops.write = dev_null_write;
    g_dev_null.cdev.ops.ioctl = NULL;
    g_dev_null.cdev.dev.mode = 0666;
    register_device(&g_dev_null.cdev.dev);

    return g_dev_null.cdev.dev.device_number;
}
