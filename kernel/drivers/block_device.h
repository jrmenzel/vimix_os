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
/// Note that a Block_Device is the driver, it also needs the
/// actual device number (for the minor number). This is stored in buf!
struct block_device_ops
{
    /// read one block of data from the buffer.
    void (*read_buf)(struct Block_Device *bd, struct buf *b);

    /// write one block of data into the buffer.
    void (*write_buf)(struct Block_Device *bd, struct buf *b);
};

/// @brief Represents one block device.
struct Block_Device
{
    struct Device dev;

    struct block_device_ops ops;
    size_t size;  //< size in bytes
};

/// @brief To cast a Device object to a Block_Device object.
/// See container_of to see why this should be used instead of pointer casting.
// #define block_device_from_device(ptr) (struct Block_Device *)(ptr)

#define block_device_from_device(ptr) \
    container_of(ptr, struct Block_Device, dev)

/// @brief Returns the Block Device which is registered under this
/// device number or NULL if there is no matching device.
static inline struct Block_Device *get_block_device(dev_t device_number)
{
    return block_device_from_device(dev_by_device_number(device_number));
}

/// @brief Read from a block device at any location.
/// @param bdev Block device
/// @param addr_u User space address
/// @param offset Offset in block device
/// @param n Number of bytes to read
/// @return Bytes read or -1 on failure
ssize_t block_device_read(struct Block_Device *bdev, size_t addr_u,
                          size_t offset, size_t n);

/// @brief Write to a block device at any location.
/// @param bdev Block device
/// @param addr_u User space address
/// @param offset Offset in block device
/// @param n Number of bytes to write
/// @return Bytes written or -1 on failure
ssize_t block_device_write(struct Block_Device *bdev, size_t addr_u,
                           size_t offset, size_t n);
