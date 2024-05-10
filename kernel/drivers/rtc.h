/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/time.h>

// real-time clock drivers

/// @brief Get time in UNIX epoch
/// @return seconds since 01-01-1970
time_t rtc_get_time();
