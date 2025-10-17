/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/time.h>

#include <stddef.h>
#include <stdint.h>

/// @brief A calendar date.
struct tm
{
    int tm_sec;    ///< seconds after the minute [0..60]
    int tm_min;    ///< minutes after the hour [0..59]
    int tm_hour;   ///< hour of the day [0..23]
    int tm_mday;   ///< day of the month [1..31]
    int tm_mon;    ///< month [0..11]
    int tm_year;   ///< years since 1900
    int tm_wday;   ///< week day [0..6] 0 = Sunday
    int tm_yday;   ///< day of the year [0..365]
    int tm_isdst;  ///< is daylight savings time
};

/// @brief Returns the time in seconds since the UNIX epoch.
/// Meaning sesonds since 1.1.1970 (ignoring leap seconds).
/// @param tloc If not NULL, the time will also be store at the location tloc.
/// For compatibility, apps should use the return code instead.
/// @return Time in seconds since the epoch or -1 on error.
time_t time(time_t *tloc);

/// @brief Converts a time value to a calendar date. The returned struct is
/// valid until the next call to localtime().
struct tm *localtime(const time_t *timer);

int clock_gettime(clockid_t clockid, struct timespec *tp);
