/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/time.h>

// real-time clock drivers

dev_t rtc_init(struct Device_Init_Parameters *init_parameters);

/// @brief Get time in UNIX epoch
/// @return seconds since 01-01-1970
time_t rtc_get_time();
