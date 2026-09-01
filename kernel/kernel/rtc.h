/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/kernel.h>
#include <kernel/time.h>

// Generic RTC interface

struct rtc_device;

/// @brief RTC callback a driver should implement.
typedef struct timespec (*rtc_get_time_fn)(struct rtc_device *rtc);

// shared struct of all RTC drivers
struct rtc_device
{
    size_t mmio_base;
    rtc_get_time_fn get_time_fn;
};

/// @brief Called by RTC drivers to register their get_time function.
/// @param fn The function to get the current time.
void rtc_register(struct rtc_device *rtc);

/// @brief Get time in UNIX epoch
/// @return seconds since 01-01-1970
struct timespec rtc_get_time();
