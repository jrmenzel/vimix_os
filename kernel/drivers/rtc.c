/* SPDX-License-Identifier: MIT */

#include <drivers/mmio_access.h>
#include <drivers/rtc.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/major.h>

struct Device_Init_Parameters goldfish_mapping = {0};
bool rtc_is_initialized = false;

dev_t rtc_init(struct Device_Init_Parameters *init_parameters, const char *name)
{
    if (rtc_is_initialized)
    {
        return 0;
    }
    goldfish_mapping = *init_parameters;
    rtc_is_initialized = true;
    return MKDEV(RTC_MAJOR, 0);
}

// See
// https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
#define TIMER_TIME_LOW 0x00
#define TIMER_TIME_HIGH 0x04

time_t rtc_get_time()
{
    if (!rtc_is_initialized)
    {
        // no real time clock -> assume boot was at time 0 / 1.1.1970
        return seconds_since_boot();
    }

    uint32_t t_low;  // unsigned !
    int32_t t_high;  // signed !
    t_low = MMIO_READ_UINT_32(goldfish_mapping.mem[0].start, TIMER_TIME_LOW);
    t_high = MMIO_READ_UINT_32(goldfish_mapping.mem[0].start, TIMER_TIME_HIGH);

    int64_t nsec = ((int64_t)t_high << 32) | (int64_t)t_low;
    return nsec / 1000000000;
}
