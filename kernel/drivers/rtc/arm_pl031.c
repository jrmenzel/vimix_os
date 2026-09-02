/* SPDX-License-Identifier: MIT */

#include <drivers/driver.h>
#include <drivers/rtc/arm_pl031.h>
#include <kernel/rtc.h>

REGISTER_DRIVER("arm,pl031", arm_pl031_init);

struct timespec arm_pl031_rtc_get_time(struct rtc_device *rtc);

atomic_size_t g_pl031_next_minor = 0;

dev_t arm_pl031_init(struct Device_Init_Parameters *init_parameters,
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
    rtc->get_time_fn = arm_pl031_rtc_get_time;

    rtc_register(rtc);

    size_t minor = (size_t)atomic_fetch_add(&g_pl031_next_minor, 1);
    return MKDEV(ARM_PL031_MAJOR, minor);
}

struct timespec arm_pl031_rtc_get_time(struct rtc_device *rtc)
{
    // the ARM PL031 has only a 32-bit time value in seconds
    uint32_t seconds = MMIO_READ_UINT_32(rtc->mmio_base, RTCDR);

    struct timespec time;
    time.tv_sec = seconds;
    time.tv_nsec = 0;

    return time;
}
