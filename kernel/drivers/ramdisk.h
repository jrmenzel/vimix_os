/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/block_device.h>
#include <drivers/devices_list.h>

/// @brief Adds itself to the devices list.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @returns the device ID of the created ram disk
dev_t ramdisk_init(struct Device_Init_Parameters *init_parameters,
                   const char *name);