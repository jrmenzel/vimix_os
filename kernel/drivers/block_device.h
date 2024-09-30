/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/device.h>
#include <kernel/buf.h>
#include <kernel/types.h>

///
/// @brief Block Device interfaces.
///

struct Block_Device;

/// @brief What a block device driver needs to implement:
struct block_device_ops
{
    /// read one block of data from the buffer.
    void (*read)(struct Block_Device *bd, struct buf *b);

    /// write one block of data into the buffer.
    void (*write)(struct Block_Device *bd, struct buf *b);
};

/// @brief Represents one block device.
struct Block_Device
{
    struct Device dev;

    struct block_device_ops ops;
};

/// @brief To cast a Device object to a Block_Device object.
/// See container_of to see why this should be used instead of pointer casting.
// #define block_device_from_device(ptr) (struct Block_Device *)(ptr)

// fix compile issues to use saver version:
#define block_device_from_device(ptr) \
    container_of(ptr, struct Block_Device, dev)

/// @brief Returns the Block Device which is registered under this
/// device number or NULL if there is no matching device.
struct Block_Device *get_block_device(dev_t device_number);
