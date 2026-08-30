/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>

// ARM PL031 RTC driver, found for example on the qemu virt board

dev_t arm_pl031_init(struct Device_Init_Parameters *init_parameters,
                     const char *name);

struct arm_pl031
{
    struct spinlock arm_pl031_lock;
    size_t rtc_base;
};

struct timespec arm_pl031_rtc_get_time();
