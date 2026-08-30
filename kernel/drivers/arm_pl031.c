/* SPDX-License-Identifier: MIT */

#include <drivers/arm_pl031.h>
#include <drivers/mmio_access.h>
#include <kernel/major.h>
#include <kernel/rtc.h>

REGISTER_DRIVER("arm,pl031", arm_pl031_init);

struct arm_pl031 g_arm_pl031;

struct timespec arm_pl031_rtc_get_time();

dev_t arm_pl031_init(struct Device_Init_Parameters *init_parameters,
                     const char *name)
{
    (void)name;

    if (g_arm_pl031.rtc_base != 0)
    {
        return INVALID_DEVICE;
    }

    if (init_parameters == NULL || init_parameters->dtb == NULL)
    {
        return INVALID_DEVICE;
    }

    g_arm_pl031.rtc_base = init_parameters->mem[0].start_va;

    rtc_register_get_time_fn(&arm_pl031_rtc_get_time);

    return MKDEV(ARM_PL031_MAJOR, 0);
}

struct timespec arm_pl031_rtc_get_time()
{
    uint32_t seconds = MMIO_READ_UINT_32(g_arm_pl031.rtc_base, 0x00);

    struct timespec time;
    time.tv_sec = seconds;
    time.tv_nsec = 0;

    return time;
}
