/* SPDX-License-Identifier: MIT */

#include <kernel/kticks.h>
#include <kernel/rtc.h>

static rtc_get_time_fn g_rtc_get_time_fn_ptr = NULL;

void rtc_register_get_time_fn(rtc_get_time_fn fn)
{
    g_rtc_get_time_fn_ptr = fn;
}

struct timespec rtc_get_time()
{
    if (g_rtc_get_time_fn_ptr == NULL)
    {
        // no real time clock -> assume boot was at time 0 / 1.1.1970
        struct timespec time;
        time.tv_sec = seconds_since_boot();
        time.tv_nsec = 0;
        return time;
    }

    return g_rtc_get_time_fn_ptr();
}
