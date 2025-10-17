/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/time.h>

// real-time clock drivers

/// @brief Inits the RTC.
/// @param init_parameters
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t rtc_init(struct Device_Init_Parameters *init_parameters,
               const char *name);

/// @brief Get time in UNIX epoch
/// @return seconds since 01-01-1970
struct timespec rtc_get_time();
