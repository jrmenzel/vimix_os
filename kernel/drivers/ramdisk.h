/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/block_device.h>

/// @brief Adds itself to the devices list.
/// @returns the device ID of the created ram disk
dev_t ramdisk_init(void *start_address, size_t size);