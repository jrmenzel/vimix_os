/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

/// @brief A time value to store the seconds since the UNIX epoch.
/// A 32 bit value would overflow on Jan 19th, 2038.
typedef int64_t time_t;

/// @brief A point in time with nanosecond resolution.
struct timespec
{
    time_t tv_sec;    ///< seconds
    int64_t tv_nsec;  ///< nanoseconds [0..999999999]
};
