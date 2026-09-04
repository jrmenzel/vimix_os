/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/container_of.h>
#include <kernel/kobject.h>
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
    BLOCK,
    OTHER  ///< For devices which do not appear in /dev
};
typedef enum device_type_enum device_type;

#define DEVICE_MAX_INTERRUPTS (16)

struct Device_Interrupt
{
    uint32_t parent_phandle;
    int32_t irq;
    uint32_t flags;
};

/// @brief Base for all devices.
/// Devices react with dev_ops->interrupt_handler() on interrupt irq_number.
struct Device
{
    struct kobject kobj;

    device_type type;
    /// @brief Interrupt ReQuest number the device reacts to
    // int32_t irq_number;
    struct Device_Interrupt interrupts[DEVICE_MAX_INTERRUPTS];
    size_t interrupt_count;

    struct general_device_ops dev_ops;

    dev_t device_number;  ///< Major and Minor device number, use MKDEV macro

    const char *name;  ///< for DEV FS in /dev
    mode_t mode;       ///< access mode for the device file
};

#define device_from_kobj(ptr) container_of(ptr, struct Device, kobj)

void dev_init(struct Device *dev, device_type type, dev_t device_number,
              const char *name, struct Device_Interrupt *irqs, size_t irq_count,
              interrupt_handler_p interrupt_handler);

struct Device *dev_by_device_number(dev_t device_number);

struct Device *dev_by_irq_number(int32_t irq_number);

/// @brief Every driver has to call this and register the device.
/// @param dev the device to register
void register_device(struct Device *dev);

/// @brief Check if a device with the given device number exists.
/// @param device_number Number to check.
/// @return True if the device exists.
bool dev_exists(dev_t device_number);
