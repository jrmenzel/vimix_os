/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>

// real-time clock drivers

/// @brief Inits the RTC.
/// @param init_parameters
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t goldfish_rtc_init(struct Device_Init_Parameters *init_parameters,
                        const char *name);
