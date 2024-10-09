/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/major.h>
#include <kernel/types.h>

struct Device;

/// @brief Interrupt handler function pointer
/// @param dev The device to call (minor number)
typedef void (*interrupt_handler_p)(dev_t dev);

/// @brief Device operations / functions that all devices have to implement.
struct general_device_ops
{
    /// @brief Interrupt handler of the device
    interrupt_handler_p interrupt_handler;
};

enum device_type_enum
{
    CHAR,
    BLOCK
};
typedef enum device_type_enum device_type;

/// @brief Base for all devices.
/// Devices react with dev_ops->interrupt_handler() on interrupt irq_number.
struct Device
{
    device_type type;
    /// @brief Interrupt ReQuest number the device reacts to
    int32_t irq_number;
    struct general_device_ops dev_ops;

    dev_t device_number;  ///< Major and Minor device number, use MKDEV macro
};

/// @brief Array of pointers to all devices in the system.
/// Add new devices at boot via register_device()
extern struct Device *g_devices[MAX_DEVICES * MAX_MINOR_DEVICES];

#define DEVICE_INDEX(major, minor) ((major) * MAX_MINOR_DEVICES + (minor))

#define INVALID_IRQ_NUMBER (-1)
/// @brief Initialize the device, called from the character and block device
/// init functions.
/// @param dev Device to init
/// @param irq_number IRQ to listen to or INVALID_IRQ_NUMBER
/// @param interrupt_handler valid interrupt callback or NULL (irq_number must
/// be INVALID_IRQ_NUMBER)
void dev_set_irq(struct Device *dev, int32_t irq_number,
                 interrupt_handler_p interrupt_handler);

/// @brief Every driver has to call this and register the device.
/// @param dev the device to register
void register_device(struct Device *dev);
