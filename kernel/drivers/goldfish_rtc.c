/* SPDX-License-Identifier: MIT */

#include <drivers/goldfish_rtc.h>
#include <drivers/mmio_access.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/major.h>
#include <kernel/pgtable.h>
#include <kernel/rtc.h>

REGISTER_DRIVER("google,goldfish-rtc", goldfish_rtc_init);

struct Device_Init_Parameters goldfish_mapping = {0};
bool rtc_is_initialized = false;

struct timespec goldfish_rtc_get_time();

dev_t goldfish_rtc_init(struct Device_Init_Parameters *init_parameters,
                        const char *name)
{
    if (rtc_is_initialized)
    {
        return INVALID_DEVICE;
    }
    goldfish_mapping = *init_parameters;
    goldfish_mapping.mem[0].start_pa = goldfish_mapping.mem[0].start_va;
    rtc_is_initialized = true;

    rtc_register_get_time_fn(&goldfish_rtc_get_time);

    return MKDEV(RTC_MAJOR, 0);
}

// See
// https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
#define TIMER_TIME_LOW 0x00
#define TIMER_TIME_HIGH 0x04

struct timespec goldfish_rtc_get_time()
{
    uint32_t t_low;  // unsigned !
    int32_t t_high;  // signed !
    t_low = MMIO_READ_UINT_32(goldfish_mapping.mem[0].start_pa, TIMER_TIME_LOW);
    t_high =
        MMIO_READ_UINT_32(goldfish_mapping.mem[0].start_pa, TIMER_TIME_HIGH);

    int64_t time_in_nsec = ((int64_t)t_high << 32) | (int64_t)t_low;

    struct timespec time;
    time.tv_sec = time_in_nsec / 1000000000;
    time.tv_nsec = time_in_nsec % 1000000000;

    return time;
}
