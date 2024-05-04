/* SPDX-License-Identifier: MIT */

#include <drivers/block_device.h>
#include <drivers/character_device.h>
#include <drivers/device.h>
#include <kernel/kernel.h>

/// @brief all devices indexed by the major device number
struct Device *g_devices[MAX_DEVICES] = {NULL};

void register_device(struct Device *dev)
{
    size_t major = MAJOR(dev->device_number);

    if (major >= MAX_DEVICES)
    {
        panic("invalid high device number");
    }
    if (g_devices[major] != NULL)
    {
        panic("multiple drivers with same device number");
    }

    // printk("register device %d\n", dev->device_number);
    g_devices[major] = dev;
}

void dev_set_irq(struct Device *dev, int32_t irq_number,
                 interrupt_handler_p interrupt_handler)
{
    dev->irq_number = irq_number;
    dev->dev_ops.interrupt_handler = interrupt_handler;
}

#ifdef CONFIG_DEBUG_EXTRA_RUNTIME_TESTS
#define DEVICE_IS_OK(major, TYPE)                               \
    if ((major >= MAX_DEVICES) || (g_devices[major] == NULL) || \
        (g_devices[major]->type != TYPE))                       \
    {                                                           \
        panic("no device of that type");                        \
        return NULL;                                            \
    }
#else
#define DEVICE_IS_OK(major, TYPE)
#endif

struct Block_Device *get_block_device(dev_t device_number)
{
    size_t major = MAJOR(device_number);

    DEVICE_IS_OK(major, BLOCK);

    return block_device_from_device(g_devices[major]);
}

struct Character_Device *get_character_device(dev_t device_number)
{
    size_t major = MAJOR(device_number);

    DEVICE_IS_OK(major, CHAR);

    return character_device_from_device(g_devices[major]);
}
