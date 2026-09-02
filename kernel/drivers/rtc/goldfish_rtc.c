/* SPDX-License-Identifier: MIT */

#include <drivers/driver.h>
#include <drivers/rtc/goldfish_rtc.h>
#include <kernel/rtc.h>

REGISTER_DRIVER("google,goldfish-rtc", goldfish_rtc_init);

struct timespec goldfish_rtc_get_time(struct rtc_device *rtc);

atomic_size_t g_goldfish_next_minor = 0;

dev_t goldfish_rtc_init(struct Device_Init_Parameters *init_parameters,
                        const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    struct rtc_device *rtc =
        kmalloc(sizeof(struct rtc_device), ALLOC_FLAG_ZERO_MEMORY);
    if (rtc == NULL)
    {
        return INVALID_DEVICE;
    }
    rtc->mmio_base = init_parameters->mem[0].start_va;
    rtc->get_time_fn = goldfish_rtc_get_time;

    rtc_register(rtc);

    size_t minor = (size_t)atomic_fetch_add(&g_goldfish_next_minor, 1);
    return MKDEV(RTC_MAJOR, minor);
}

struct timespec goldfish_rtc_get_time(struct rtc_device *rtc)
{
    uint32_t t_low;  // unsigned !
    int32_t t_high;  // signed !
    t_low = MMIO_READ_UINT_32(rtc->mmio_base, TIMER_TIME_LOW);
    t_high = MMIO_READ_UINT_32(rtc->mmio_base, TIMER_TIME_HIGH);

    int64_t time_in_nsec = ((int64_t)t_high << 32) | (int64_t)t_low;

    struct timespec time;
    time.tv_sec = time_in_nsec / 1000000000;
    time.tv_nsec = time_in_nsec % 1000000000;

    return time;
}
