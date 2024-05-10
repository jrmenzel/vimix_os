/* SPDX-License-Identifier: MIT */

#include <drivers/rtc.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

#ifdef RTC_GOLDFISH

// See
// https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
#define TIMER_TIME_LOW 0x00
#define TIMER_TIME_HIGH 0x04
#define TimeReg(reg) ((volatile uint32_t *)(RTC_GOLDFISH + reg))
#define ReadTimeReg(reg) (*(TimeReg(reg)))

time_t rtc_get_time()
{
    uint32_t t_low;  // unsigned !
    int32_t t_high;  // signed !
    int64_t nsec;

    t_low = ReadTimeReg(TIMER_TIME_LOW);
    t_high = ReadTimeReg(TIMER_TIME_HIGH);
    nsec = ((int64_t)t_high << 32) | (int64_t)t_low;

    return nsec / 1000000000;
}

#else

// dummy if no RTC is present
time_t rtc_get_time() { return 0; }

#endif
