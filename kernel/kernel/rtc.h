/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/time.h>

// Generic RTC interface

/// @brief RTC callback a driver should implement.
typedef struct timespec (*rtc_get_time_fn)();

/// @brief Called by RTC drivers to register their get_time function.
/// @param fn The function to get the current time.
void rtc_register_get_time_fn(rtc_get_time_fn fn);

/// @brief Get time in UNIX epoch
/// @return seconds since 01-01-1970
struct timespec rtc_get_time();
