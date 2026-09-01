/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/rtc.h>

// real-time clock driver found in RISC V qemu
// https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT

#define TIMER_TIME_LOW 0x00
#define TIMER_TIME_HIGH 0x04

/// @brief Inits the RTC.
/// @param init_parameters MMIO parameters.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t goldfish_rtc_init(struct Device_Init_Parameters *init_parameters,
                        const char *name);
