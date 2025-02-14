/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/device.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>

struct Character_Device;

typedef ssize_t (*DEVICE_READ_FUNCTION)(struct Device *dev,
                                        bool addr_is_userspace, size_t addr,
                                        size_t len);
typedef ssize_t (*DEVICE_WRITE_FUNCTION)(struct Device *dev,
                                         bool addr_is_userspace, size_t addr,
                                         size_t len);

typedef int32_t (*DEVICE_IOCTL_FUNCTION)(struct inode *, int, void *);

/// @brief What a character device driver needs to implement:
///        read/write to a buffer which might be in userspace
///        ioctl is optional (can be NULL)
struct char_device_ops
{
    DEVICE_READ_FUNCTION read;
    DEVICE_WRITE_FUNCTION write;
    DEVICE_IOCTL_FUNCTION ioctl;
};

/// @brief Represents a character device.
/// Linux calls its counterpart cdev.
struct Character_Device
{
    struct Device dev;

    struct char_device_ops ops;
};

/// @brief To cast a device object to a Character_Device object.
/// See container_of docu to see why this should be used instead of pointer
/// casting.
// #define character_device_from_device(ptr) (struct Character_Device *)(ptr)

#define character_device_from_device(ptr) \
    container_of(ptr, struct Character_Device, dev)

/// @brief returns a character device registered for that device number or NULL
struct Character_Device *get_character_device(dev_t device_number);

/// @brief returns a character device registered for that device major or NULL
struct Character_Device *get_character_device_by_major(size_t major);
