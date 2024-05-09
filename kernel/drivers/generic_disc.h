/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/block_device.h>
#include <kernel/types.h>

///
/// @brief Generic disc interfaces.
///

#define DISK_NAME_LEN 32

/// @brief Represents one generic disk or partition.
/// Can also be a virtual disk in a file, e.g. when run in qemu.
/// Linux calls its counterpart gendisc.
struct Generic_Disc
{
    struct Block_Device bdev;

    char disk_name[DISK_NAME_LEN];
};

/// @brief To cast a Device object to a Generic_Disc object.
/// See container_of to see why this should be used instead of pointer casting.
#define generic_disk_from_block_device(ptr) \
    container_of(ptr, struct Generic_Disc, dev)
