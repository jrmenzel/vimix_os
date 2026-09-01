/* SPDX-License-Identifier: MIT */

#include <kernel/kticks.h>
#include <kernel/rtc.h>

struct rtc_device *g_rtc = NULL;

void rtc_register(struct rtc_device *rtc)
{
    DEBUG_EXTRA_PANIC(g_rtc == NULL, "Only one RTC is supported");
    g_rtc = rtc;
}

struct timespec rtc_get_time()
{
    if (g_rtc == NULL)
    {
        // no real time clock -> assume boot was at time 0 / 1.1.1970
        struct timespec time;
        time.tv_sec = seconds_since_boot();
        time.tv_nsec = 0;
        return time;
    }

    return g_rtc->get_time_fn(g_rtc);
}
