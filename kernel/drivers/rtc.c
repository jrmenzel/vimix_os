/* SPDX-License-Identifier: MIT */

#include <drivers/rtc.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <mm/memlayout.h>

struct Device_Memory_Map goldfish_mapping = {0};
bool rtc_is_initialized = false;

dev_t rtc_init(struct Device_Memory_Map *mapping)
{
    if (rtc_is_initialized)
    {
        return 0;
    }
    goldfish_mapping = *mapping;
    rtc_is_initialized = true;
    return MKDEV(RTC_MAJOR, 0);
}

// See
// https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
#define TIMER_TIME_LOW 0x00
#define TIMER_TIME_HIGH 0x04
#define TimeReg(reg) ((volatile uint32_t *)(goldfish_mapping.mem_start + reg))
#define ReadTimeReg(reg) (*(TimeReg(reg)))

time_t rtc_get_time()
{
    if (!rtc_is_initialized)
    {
        return 0;
    }

    uint32_t t_low;  // unsigned !
    int32_t t_high;  // signed !
    int64_t nsec;

    t_low = ReadTimeReg(TIMER_TIME_LOW);
    t_high = ReadTimeReg(TIMER_TIME_HIGH);
    nsec = ((int64_t)t_high << 32) | (int64_t)t_low;

    return nsec / 1000000000;
}
